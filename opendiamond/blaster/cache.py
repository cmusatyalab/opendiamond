#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2012 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from future import standard_library
standard_library.install_aliases()
from builtins import object
from collections import Mapping
import pickle as pickle
from datetime import datetime
from hashlib import sha256
import logging
import os
import shutil
from tempfile import NamedTemporaryFile
import zipfile

import dateutil.parser
from dateutil.tz import tzutc

_log = logging.getLogger(__name__)


def _attr_key_to_member(key):
    if key == '':
        return '__OBJECT_DATA__'
    return key


def _member_to_attr_key(name):
    if name == '__OBJECT_DATA__':
        return ''
    return name


class SearchCacheLoadError(Exception):
    pass


class _CachedSearchResult(Mapping):
    def __init__(self, path):  # pylint: disable=super-init-not-called
        self._zip = zipfile.ZipFile(path, 'r')

    def __len__(self):
        return len(self._zip.namelist())

    def __iter__(self):
        return (_member_to_attr_key(n) for n in self._zip.namelist())

    def __contains__(self, key):
        return _attr_key_to_member(key) in self._zip.namelist()

    def __getitem__(self, key):
        return self._zip.read(_attr_key_to_member(key))


class SearchCache(object):
    '''Assumes single-threaded, single-process access to the cache
    (except for pruning).'''

    def __init__(self, path):
        if not os.path.exists(path):
            os.makedirs(path)
        self._basedir = path

    def _search_dir_path(self, search_key):
        return os.path.join(self._basedir, search_key)

    def _search_path(self, search_key):
        return os.path.join(self._search_dir_path(search_key), 'search')

    def _search_expiration_path(self, search_key):
        return os.path.join(self._search_dir_path(search_key), 'expires')

    def _object_path(self, search_key, object_key):
        return os.path.join(self._search_dir_path(search_key), object_key)

    def _object_key(self, object_id):
        return sha256(object_id).hexdigest()

    def _hash_file(self, filh):
        filh.seek(0)
        hash = sha256()
        while True:
            buf = filh.read(131072)
            if buf == '':
                break
            hash.update(buf)
        return hash.hexdigest()

    def put_search(self, obj, expiration):
        '''obj is an application-defined search object.  expiration is a
        timezone-aware datetime specifying when the search expires.'''

        obj_fh = NamedTemporaryFile(dir=self._basedir, delete=False)
        pickle.dump(obj, obj_fh, pickle.HIGHEST_PROTOCOL)
        search_key = self._hash_file(obj_fh)
        obj_fh.close()

        exp_fh = NamedTemporaryFile(dir=self._basedir, delete=False)
        exp_fh.write(expiration.isoformat() + '\n')
        exp_fh.close()

        dirpath = self._search_dir_path(search_key)
        filepath = self._search_path(search_key)
        if not os.path.exists(dirpath):
            os.makedirs(dirpath)
        if not os.path.exists(filepath):
            os.rename(exp_fh.name, self._search_expiration_path(search_key))
            os.rename(obj_fh.name, filepath)
        else:
            os.unlink(exp_fh.name)
            os.unlink(obj_fh.name)
        return search_key

    def get_search(self, search_key):
        try:
            with open(self._search_path(search_key), 'rb') as fh:
                return pickle.load(fh)
        except IOError:
            raise KeyError()
        except Exception:
            raise SearchCacheLoadError()

    def put_search_result(self, search_key, object_id, result):
        '''result is a dict of object attributes.'''
        fh = NamedTemporaryFile(dir=self._basedir, delete=False)
        zf = zipfile.ZipFile(fh, 'w', zipfile.ZIP_STORED, True)
        for k, v in result.items():
            zf.writestr(_attr_key_to_member(k), v)
        zf.close()
        fh.close()
        object_key = self._object_key(object_id)
        os.rename(fh.name, self._object_path(search_key, object_key))
        return object_key

    def get_search_result(self, search_key, object_key):
        try:
            return _CachedSearchResult(
                self._object_path(search_key, object_key))
        except IOError:
            raise KeyError()

    def prune(self):
        '''May be run in a different thread.'''
        now = datetime.now(tzutc())
        expired = 0
        for search_key in os.listdir(self._basedir):
            # search_key may or may not be a search key; we have to check
            # the filesystem to make sure
            exp_path = self._search_expiration_path(search_key)
            try:
                with open(exp_path, 'r') as fh:
                    exp_time = dateutil.parser.parse(fh.read())
            except IOError:
                # No expiration file ==> not a search
                continue
            if exp_time < now:
                shutil.rmtree(self._search_dir_path(search_key))
                expired += 1
        if expired:
            _log.info('Expired %d searches', expired)
