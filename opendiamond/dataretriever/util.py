#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from builtins import object
import logging
import math
import os
import pwd
import grp
import random
import re
import sys
from tempfile import mkstemp

ATTR_SUFFIX = '.text_attr'

_log = logging.getLogger(__name__)

class DiamondTextAttr(object):
    """
    DEPRECIATED: this legacy format to store object attributes is deprecated.
    JSON is recommended for new uses.
    Suggest used as context manager.
    Notice: attribute values are all treated as strings.
    Remember to type cast in caller as necessary."""

    def __init__(self, path, mode, suffix=ATTR_SUFFIX):
        self.path = path + suffix
        self.mode = mode

    def __enter__(self):
        if 'w' in self.mode:
            fd, name = mkstemp(suffix=ATTR_SUFFIX+'-tmp',
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
    def exists(path, suffix=ATTR_SUFFIX):
        return os.path.isfile(path + suffix)


def read_file_list(path):
    sys.stdout.flush()
    if not os.path.exists(path):
        sys.exit('Error: Path {} does not exist'.format(path))

    with open(path,'r') as f:
        data = f.read().splitlines()
    return [d.strip() for d in data]

def write_data(path, lists_, seed):
    _log.info("Writing data for path {}".format(path))
    uid = pwd.getpwnam("dataretriever").pw_uid
    gid = grp.getgrnam("dataretriever").gr_gid
    data = []
    for l in lists_:
        data.extend(l)
    random.Random(seed).shuffle(data)
    with open(path,'w') as f:
        for d in data:
            print(d, file=f)
    os.chown(path, uid, gid)
    return data