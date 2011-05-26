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

from hashlib import md5
import os
from tempfile import mkstemp

class BlobCache(object):
    def __init__(self, basedir):
        self.basedir = basedir

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
                # Destination already exists.  We don't want to clobber it
                # because that might unset its executable bit.
                pass
        finally:
            os.unlink(name)

    def executable_path(self, sig):
        try:
            path = self._path(sig)
            st = os.stat(path)
            if (st.st_mode & 0100) == 0:
                os.chmod(path, 0500)
            return path
        except OSError:
            # stat failed, object not present in cache
            raise KeyError()
