#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2002-2007 Intel Corporation
#  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
#  Copyright (c) 2006-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from hashlib import md5
import os
from tempfile import NamedTemporaryFile

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

    def add(self, data, executable=False):
        sig = md5(data).hexdigest()
        temp = NamedTemporaryFile(dir=self.basedir, delete=False)
        if executable:
            os.fchmod(temp.fileno(), 0700)
        temp.write(data)
        temp.close()
        os.rename(temp.name, self._path(sig))

    def path(self, sig):
        if sig not in self:
            raise KeyError()
        return self._path(sig)
