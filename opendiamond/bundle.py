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
from xml.dom import minidom
from xml.etree.ElementTree import ElementTree, Element
import zipfile

BUNDLE_NS = 'http://diamond.cs.cmu.edu/xmlns/opendiamond/bundle-1'

def element(element_name, *children, **attrs):
    '''Return an XML element with the specified name, attributes, and
    children.'''
    el = Element(element_name)
    for k, v in attrs.iteritems():
        # Allow caller to specify an attribute value of None to skip the
        # attribute
        if v is not None:
            el.set(k, _xmlattr(v))
    for child in children:
        # Allow caller to specify None when it does not wish to provide an
        # optional child
        if child is not None:
            el.append(child)
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
    '''Write a predicate or codec bundle to the file specified in out.  Codec
    bundles must have a ".codec" extension; predicates ".pred".  manifest is
    the XML manifest for the bundle as a string.  files is a dict of
    filename => path pairs.'''
    zip = zipfile.ZipFile(out, mode='w', compression=zipfile.ZIP_DEFLATED)
    zip.writestr('opendiamond-bundle.xml', manifest)
    for name, path in files.iteritems():
        zip.write(path, name)
    zip.close()


def bundle_macro(out, display_name, filter, arguments, files):
    '''Produce a basic predicate bundle wrapping a macro to be interpreted by
    the specified filter (such as fil_imagej_exec or fil_matlab_exec).
    The only predicate options will be the minimum and maximum scores (ranging
    from 0 to 100), both of which will be optional.  The specified list of
    constant arguments will be passed to the filter.  The blob argument will
    be a Zip file containing the specified files under their original names.
    The predicate will depend on the RGB filter.'''
    filemap = dict((os.path.basename(f), f) for f in files)
    manifest = element('predicate',
        element('options',
            element('numberOption', displayName='Minimum score',
                    name='minScore', default=1, min=0, max=100, step=.05,
                    initiallyEnabled=True, disabledValue=float('-inf')),
            element('numberOption', displayName='Maximum score',
                    name='maxScore', default=1, min=0, max=100, step=.05,
                    initiallyEnabled=False, disabledValue=float('inf')),
        ),
        element('filters',
            element('filter',
                element('minScore', option='minScore'),
                element('maxScore', option='maxScore'),
                element('dependencies',
                    element('dependency', fixedName='RGB'),
                ),
                element('arguments',
                    *[element('argument', value=v) for v in arguments]
                ),
                element('blob',
                    *[element('member', filename=f, data=f) for f in
                            filemap.iterkeys()]
                ),
                code=filter,
            ),
        ),
        xmlns=BUNDLE_NS,
        displayName=display_name,
    )
    bundle_generic(out, format_manifest(manifest), filemap)
