#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2017 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from future import standard_library
standard_library.install_aliases()
from io import StringIO

import pytest

import opendiamond.bundle


# smallest manifest that managed to pass the validator
NULL_MANIFEST = """\
<?xml version='1.0' encoding='UTF-8'?>
<predicate xmlns="http://diamond.cs.cmu.edu/xmlns/opendiamond/bundle-1" displayName="null">
  <filters>
    <filter code="fil_null"/>
  </filters>
</predicate>
"""  # noqa


def test_format_manifest():
    root = opendiamond.bundle.element(
        'predicate',
        opendiamond.bundle.element(
            'filters',
            opendiamond.bundle.element('filter', code='fil_null')
        ),
        displayName="null"
    )
    assert opendiamond.bundle.format_manifest(root) == NULL_MANIFEST


def test_parse_manifest():
    assert opendiamond.bundle.parse_manifest(NULL_MANIFEST) is not None


def test_validate_manifest():
    # We've tested good input with parse_manifest, now test some bad inputs
    with pytest.raises(TypeError) as e:
        assert opendiamond.bundle.validate_manifest('')
    e.match('Invalid input object: str')

    root = opendiamond.bundle.element('bad')
    with pytest.raises(opendiamond.bundle.InvalidManifest) as e:
        assert opendiamond.bundle.validate_manifest(root)
    e.match('No matching global declaration available')

    root = opendiamond.bundle.element('predicate')
    with pytest.raises(opendiamond.bundle.InvalidManifest) as e:
        assert opendiamond.bundle.validate_manifest(root)
    e.match("The attribute 'displayName' is required but missing.")

    root = opendiamond.bundle.element(
        'predicate',
        opendiamond.bundle.element('options'),
        displayName="test"
    )
    with pytest.raises(opendiamond.bundle.InvalidManifest) as e:
        assert opendiamond.bundle.validate_manifest(root)
    e.match('Missing child element')


def test_bundle_macro():
    out = StringIO()
    opendiamond.bundle.bundle_macro(
        out, 'test filter', 'fil_test', ['arg1', 'arg2'], files=[])
