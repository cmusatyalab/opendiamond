#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''On-disk caching of filter code and blob arguments.'''

import os
import shutil
from tempfile import mktemp, mkstemp, mkdtemp

from opendiamond.helpers import md5

class BlobCache(object):
    '''A persistent cache of binary data identified by its MD5 hash in hex.'''

    def __init__(self, basedir):
        self.basedir = basedir
        # Ensure _executable_dir is inside the search-specific tempdir
        self._executable_dir = mkdtemp(dir=os.environ.get('TMPDIR'),
                                        prefix='executable-')

    def _path(self, sig):
        return os.path.join(self.basedir, sig.lower())

    def __contains__(self, sig):
        return os.path.exists(self._path(sig))

    def __getitem__(self, sig):
        try:
            return open(self._path(sig), 'rb').read()
        except IOError:
            raise KeyError()

    def add(self, data):
        '''Add the specified data to the cache.'''
        sig = md5(data).hexdigest()
        # NamedTemporaryFile always deletes the file on close on Python 2.5,
        # so we can't use it
        fd, name = mkstemp(dir=self.basedir)
        try:
            temp = os.fdopen(fd, 'r+')
            temp.write(data)
            temp.close()
            os.chmod(name, 0400)
            try:
                os.link(name, self._path(sig))
            except OSError:
                # Destination already exists.
                pass
        finally:
            os.unlink(name)

    def executable_path(self, sig):
        '''Return a path to the file containing the specified data
        so that the file can be executed.  Ensure that the executable
        bit is set in the filesystem.  The path is guaranteed to be valid
        for the lifetime of the search, even in the presence of
        garbage-collection.'''
        src = self._path(sig)
        dest = os.path.join(self._executable_dir, sig)
        if not os.path.exists(dest):
            # Link the blob into a temporary directory.  This directory will
            # normally (but need not always) be deleted by the supervisor when
            # the search terminates.
            try:
                try:
                    # We have to use mktemp() here because link() requires the
                    # destination not to exist.  If someone else wins the race
                    # to create the same file we'll simply fall back to
                    # copying it.
                    dest_tmp = mktemp(dir=self._executable_dir)
                    os.link(src, dest_tmp)
                except OSError:
                    # self._executable_dir may be on a different filesystem
                    # than self.basedir.  Try copying the file instead.
                    fd, dest_tmp = mkstemp(dir=self._executable_dir)
                    os.close(fd)
                    shutil.copyfile(src, dest_tmp)
                os.chmod(dest_tmp, 0500)
                os.rename(dest_tmp, dest)
            except (OSError, IOError):
                # Object not present in cache
                raise KeyError()
        return dest
