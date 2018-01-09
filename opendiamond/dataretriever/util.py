#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import os
import re
from tempfile import mkstemp

_SUFFIX = '.text_attr'


class DiamondTextAttr(object):
    """
    DEPRECIATED: this legacy format to store object attributes is deprecated.
    JSON is recommended for new uses.
    Suggest used as context manager.
    Notice: attribute values are all treated as strings.
    Remember to type cast in caller as necessary."""

    def __init__(self, path, mode, suffix=_SUFFIX):
        self.path = path + suffix
        self.mode = mode

    def __enter__(self):
        if 'w' in self.mode:
            fd, name = mkstemp(suffix=_SUFFIX+'-tmp',
                               prefix='diamond-',
                               dir=os.path.dirname(self.path))
            self.file = os.fdopen(fd, self.mode)
            self.temp_name = name
        else:
            self.file = open(self.path, self.mode)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.file.close()
        if 'w' in self.mode:
            try:
                os.link(self.temp_name, self.path)
            except OSError:
                # First writer wins
                pass
            finally:
                os.unlink(self.temp_name)

    def __iter__(self):
        for line in self.file:
            m = re.match(r'^\s*"([^"]+)"\s*=\s*"([^"]*)"', line)
            if not m:
                continue
            yield m.groups()

    def write(self, key, value):
        self.file.write('"{key}"="{value}"\n'.format(key=key, value=value))

    @staticmethod
    def exists(path, suffix=_SUFFIX):
        return os.path.isfile(path + suffix)
