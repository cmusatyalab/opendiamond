#
#  The OpenDiamond Platform for Interactive Search
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

from builtins import object
from hashlib import sha256
import logging
import os
import shutil
from tempfile import mktemp, mkstemp, mkdtemp
import time

GC_SUFFIX = '-'

_log = logging.getLogger(__name__)


class BlobCache(object):
    '''A cache of binary data identified by its SHA256 hash in hex.

    A blob is eligible for garbage collection if its mtime is older than the
    configured lifetime.  Whenever we access a blob (by reading it or via
    __contains__()), we update its mtime.

    Garbage collection runs in two passes.  In the first pass, we check
    the mtime of every blob and rename expired blobs from %s to %s-.
    In the second pass, we stat every file named %s- and unlink it if
    it is still expired.  The second pass handles the case where the
    mtime has been updated between the stat and rename in the first pass.
    Since %s- blobs cannot have their mtime updated, if the mtime is
    still expired in the second pass, we know it is safe to delete.

    When accessing an object:
    1.  We update its mtime.  If this succeeds, go to step 2.  Otherwise,
    we may be able to rescue the object from a pending GC.
    1a.  To rescue the blob, we rename %s- to %s.  This may fail due to
    races with other threads accessing the object.  Therefore, regardless of
    the outcome of the rename, we attempt to update the mtime again.  If this
    fails, we assume the object does not exist.
    2.  We open the file.  If this fails, we have hit the stat/rename race
    in the garbage collector and need to rescue the file.
    2a. To rescue the file, we rename it from %s- to %s and try again.
    The second attempt should succeed since the file's mtime is current.
    '''

    def __init__(self, basedir):
        self.basedir = basedir

    def _path(self, sig):
        return os.path.join(self.basedir, sig.lower())

    def _try_with_rescue(self, sig, operation_func, retriable_exceptions):
        '''Return operation_func().  If operation_func throws one of
        retriable_exceptions, try to rescue the blob and retry
        operation_func().'''
        try:
            return operation_func()
        except retriable_exceptions:
            try:
                path = self._path(sig)
                os.rename(path + GC_SUFFIX, path)
                _log.debug('Rescued blob cache entry %s', sig)
            except OSError:
                pass
            return operation_func()

    def _access(self, sig):
        '''Update the mtime of the blob to prevent it from being
        garbage-collected.  Rescue the blob if necessary.  Raise KeyError
        if the blob is not in the cache.'''
        try:
            self._try_with_rescue(
                sig, lambda: os.utime(self._path(sig), None), OSError)
        except OSError:
            raise KeyError()

    def __contains__(self, sig):
        try:
            self._access(sig)
            return True
        except KeyError:
            return False

    # pylint is confused by the lambda expression
    # pylint: disable=unnecessary-lambda
    def __getitem__(self, sig):
        self._access(sig)
        return self._try_with_rescue(
            sig, lambda: open(self._path(sig), 'rb').read(), IOError)
    # pylint: enable=unnecessary-lambda

    def add(self, data):
        '''Add the specified data to the cache.'''
        hash = sha256(data)
        sig = hash.hexdigest()
        # NamedTemporaryFile always deletes the file on close on Python 2.5,
        # so we can't use it
        fd, name = mkstemp(dir=self.basedir)
        try:
            temp = os.fdopen(fd, 'rb+')
            temp.write(data)
            temp.close()
            os.chmod(name, 0o400)
            try:
                os.link(name, self._path(sig))
            except OSError:
                # Destination already exists.
                pass
        finally:
            os.unlink(name)
        return sig

    @classmethod
    def prune(cls, basedir, max_days):
        '''Safely remove all blobs from basedir which are older than max_days
        days.'''
        expires = time.time() - 60 * 60 * 24 * max_days
        # First, rename expired blobs
        for file in os.listdir(basedir):
            if not file.endswith(GC_SUFFIX):
                path = os.path.join(basedir, file)
                try:
                    if os.stat(path).st_mtime < expires:
                        os.rename(path, path + GC_SUFFIX)
                except OSError:
                    pass
        # Now delete renamed and expired blobs
        count = 0
        bytes = 0
        for file in os.listdir(basedir):
            if file.endswith(GC_SUFFIX):
                path = os.path.join(basedir, file)
                try:
                    st = os.stat(path)
                    if st.st_mtime < expires:
                        os.unlink(path)
                        count += 1
                        bytes += st.st_size
                except OSError:
                    pass
        # Log the results
        if count > 0:
            _log.info('Pruned %d blob cache entries, %d bytes', count, bytes)


class ExecutableBlobCache(BlobCache):
    '''A BlobCache that can create executable files from cache entries.

    Creates a temporary directory in TMPDIR and does not clean it up,
    under the expectation that the entire TMPDIR will be blown away after
    the search is complete.
    '''

    def __init__(self, basedir):
        BlobCache.__init__(self, basedir)
        # Ensure _executable_dir is inside the search-specific tempdir
        self._executable_dir = mkdtemp(dir=os.environ.get('TMPDIR'),
                                       prefix='executable-')

    def executable_path(self, sig):
        '''Return a path to the file containing the specified data
        so that the file can be executed.  Ensure that the executable
        bit is set in the filesystem.  The path is guaranteed to be valid
        for the lifetime of the search, even in the presence of
        garbage-collection.'''
        src = self._path(sig)
        dest = os.path.join(self._executable_dir, sig)

        def make_dest():
            # Link the blob into a temporary directory.  This directory will
            # normally (but need not always) be deleted by the supervisor when
            # the search terminates.
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
            os.chmod(dest_tmp, 0o500)
            os.rename(dest_tmp, dest)
        self._access(sig)
        if not os.path.exists(dest):
            self._try_with_rescue(sig, make_dest, (OSError, IOError))
        return dest
