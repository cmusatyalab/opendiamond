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

from opendiamond.helpers import md5

class Blob(object):
    '''An abstract class wrapping some binary data that will be loaded later.
    The data can be retrieved with str().'''

    def __init__(self):
        self._md5 = None

    def __str__(self):
        raise NotImplementedError()

    def __repr__(self):
        return '<Blob>'

    @property
    def md5(self):
        if self._md5 is None:
            self._md5 = md5(str(self)).hexdigest()
        return self._md5


class EmptyBlob(Blob):
    '''An empty blob argument.'''

    def __str__(self):
        return ''

    def __hash__(self):
        return 12345

    def __eq__(self, other):
        return type(self) is type(other)
