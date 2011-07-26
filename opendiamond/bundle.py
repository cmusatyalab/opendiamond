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
import os
import subprocess
from xml.etree.ElementTree import Element
import zipfile

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


def bundle_generic(out, manifest, files):
    '''manifest is a string, files is a dict of filename => path pairs'''
    zip = zipfile.ZipFile(out, mode='w', compression=zipfile.ZIP_DEFLATED)
    zip.writestr('opendiamond-search.xml', manifest)
    for name, path in files.iteritems():
        zip.write(path, name)
    zip.close()


def bundle_python(out, search, additional=None):
    try:
        proc = subprocess.Popen(['python', os.path.realpath(search),
                            '--get-manifest'], stdout=subprocess.PIPE)
    except OSError:
        raise Exception("Couldn't execute search program")
    manifest = proc.communicate()[0]
    if proc.returncode != 0:
        raise Exception("Couldn't generate search manifest")
    files = {'filter': search}
    if additional is not None:
        files.update((os.path.basename(f), f) for f in additional)
    bundle_generic(out, manifest, files)
