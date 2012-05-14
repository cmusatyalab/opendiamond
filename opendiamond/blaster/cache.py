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

import cPickle as pickle
import os
from tempfile import NamedTemporaryFile

from opendiamond.helpers import sha256

class SearchCacheLoadError(Exception):
    pass


class SearchCache(object):
    '''Assumes single-threaded, single-process access to the cache.'''

    def __init__(self, path):
        if not os.path.exists(path):
            os.makedirs(path)
        self._basedir = path

    def _search_dir_path(self, search_key):
        return os.path.join(self._basedir, search_key)

    def _search_path(self, search_key):
        return os.path.join(self._search_dir_path(search_key), 'search')

    def _hash_file(self, filh):
        filh.seek(0)
        hash = sha256()
        while True:
            buf = filh.read(131072)
            if buf == '':
                break
            hash.update(buf)
        return hash.hexdigest()

    def put_search(self, obj):
        fh = NamedTemporaryFile(dir=self._basedir, delete=False)
        pickle.dump(obj, fh, pickle.HIGHEST_PROTOCOL)
        search_key = self._hash_file(fh)
        fh.close()

        dirpath = self._search_dir_path(search_key)
        filepath = self._search_path(search_key)
        if not os.path.exists(dirpath):
            os.makedirs(dirpath)
        if not os.path.exists(filepath):
            os.rename(fh.name, filepath)
        else:
            os.unlink(fh.name)
        return search_key

    def get_search(self, search_key):
        try:
            with open(self._search_path(search_key), 'rb') as fh:
                return pickle.load(fh)
        except IOError:
            raise KeyError()
        except Exception:
            raise SearchCacheLoadError()
