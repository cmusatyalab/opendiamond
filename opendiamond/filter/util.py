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

import math
from xml.etree.ElementTree import Element

def element(name, attrs=None):
    '''Return an XML element with the specified name and attributes.'''
    el = Element(name)
    if attrs is not None:
        for k, v in attrs.iteritems():
            # Allow caller to specify an attribute value of None to skip the
            # attribute
            if v is not None:
                el.set(k, _xmlattr(v))
    return el


def _xmlattr(item):
    '''Return XML Schema representation of item.'''
    if item is True:
        return 'true'
    elif item is False:
        return 'false'
    elif isinstance(item, float):
        if math.isnan(item):
            return 'NaN'
        elif math.isinf(item) and item < 0:
            return '-INF'
        elif math.isinf(item):
            return 'INF'
    return str(item)
