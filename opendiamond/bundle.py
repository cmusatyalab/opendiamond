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

from cStringIO import StringIO
import math
import os
import subprocess
from xml.dom import minidom
from xml.etree.ElementTree import ElementTree, Element
import zipfile

BUNDLE_NS = 'http://diamond.cs.cmu.edu/xmlns/opendiamond/bundle-1'

def element(element_name, **attrs):
    '''Return an XML element with the specified name and attributes.'''
    el = Element(element_name)
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


def format_manifest(root):
    '''Given an XML root element for a bundle manifest, return the manifest
    serialized as a string.'''
    buf = StringIO()
    etree = ElementTree(root)
    etree.write(buf, encoding='UTF-8', xml_declaration=True)
    # Now run the data through minidom for pretty-printing
    dom = minidom.parseString(buf.getvalue())
    return dom.toprettyxml(indent='  ', encoding='UTF-8')


def bundle_generic(out, manifest, files):
    '''Write a search bundle to the file specified in out.  manifest is the
    XML manifest for the bundle as a string.  files is a dict of
    filename => path pairs.'''
    zip = zipfile.ZipFile(out, mode='w', compression=zipfile.ZIP_DEFLATED)
    zip.writestr('opendiamond-search.xml', manifest)
    for name, path in files.iteritems():
        zip.write(path, name)
    zip.close()


def bundle_python(out, search, additional=None):
    '''Write a search bundle for the Python filter whose path is specified
    in search to the file specified in out.  Include the additional files
    specified.'''
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
