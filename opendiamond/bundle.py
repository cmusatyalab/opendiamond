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

import math
import os
from lxml import etree
from lxml.etree import DocumentInvalid, Element, ParseError
from pkg_resources import resource_string
import zipfile

_schema_doc = etree.fromstring(resource_string(__name__, "bundle.xsd"))
_schema = etree.XMLSchema(_schema_doc)
BUNDLE_NS = _schema_doc.get('targetNamespace')
BUNDLE_NS_PFX = '{' + BUNDLE_NS + '}'


class InvalidManifest(Exception):
    pass


def element(element_name, *children, **attrs):
    '''Return an XML element in the bundle namespace with the specified
    name, attributes, and children.'''
    el = Element(BUNDLE_NS_PFX + element_name, nsmap={None: BUNDLE_NS})
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
    return etree.tostring(root, encoding='UTF-8', xml_declaration=True,
                            pretty_print=True)


def parse_manifest(data):
    '''Given a bundle manifest as a string, parse and validate it and return
    the root element.  Raise InvalidManifest if there is a problem.'''
    try:
        el = etree.fromstring(data)
        validate_manifest(el)
        return el
    except ParseError, e:
        raise InvalidManifest(str(e))


def validate_manifest(root):
    '''Given an XML root element for a bundle manifest, validate the
    manifest and raise InvalidManifest if there is a problem.'''
    try:
        _schema.assertValid(root)
    except DocumentInvalid, e:
        raise InvalidManifest(str(e))


def bundle_generic(out, root, files):
    '''Write a predicate or codec bundle to the file specified in out.  Codec
    bundles must have a ".codec" extension; predicates ".pred".  root is the
    root element of the XML manifest for the bundle.  files is a dict of
    filename => path pairs.  Raise InvalidManifest if the manifest is
    invalid.'''
    validate_manifest(root)
    zip = zipfile.ZipFile(out, mode='w', compression=zipfile.ZIP_DEFLATED)
    zip.writestr('opendiamond-bundle.xml', format_manifest(root))
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
    root = element('predicate',
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
        displayName=display_name,
    )
    bundle_generic(out, root, filemap)
