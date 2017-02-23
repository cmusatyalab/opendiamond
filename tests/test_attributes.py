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

import math
import struct

import PIL.Image
import pytest

import opendiamond.attributes


def test_string_attr():
    codec = opendiamond.attributes.StringAttributeCodec()
    assert codec.encode("test") == b'test\0'
    assert codec.decode(b'test\0') == "test"
    assert codec.decode(b'\0\0') == '\0'

    # conversion to string
    assert codec.encode(u"test") == b'test\0'
    assert codec.encode(1) == b'1\0'

    # bad inputs
    with pytest.raises(ValueError) as e:
        assert codec.decode(b'test')
    e.match('is not null-terminated')


def test_integer_attr():
    codec = opendiamond.attributes.IntegerAttributeCodec()
    assert codec.encode(1) == b'\1\0\0\0'
    assert codec.decode(b'\1\0\0\0') == 1
    assert codec.encode(-1) == b'\xff\xff\xff\xff'
    assert codec.decode(b'\xff\xff\xff\xff') == -1

    # implicit conversion to int
    assert codec.encode(1.0) == b'\1\0\0\0'

    # bad inputs
    with pytest.raises(struct.error) as e:
        assert codec.encode("test")
    e.match('cannot convert argument to integer')

    with pytest.raises(struct.error) as e:
        assert codec.decode(b'\1')
    e.match('unpack requires a string argument of length 4')


def test_float_attr():
    codec = opendiamond.attributes.DoubleAttributeCodec()

    assert codec.encode(1.0) == b'\0\0\0\0\0\0\xf0?'
    assert codec.decode(b'\0\0\0\0\0\0\xf0?') == 1.0
    assert codec.encode(float('inf')) == b'\0\0\0\0\0\0\xf0\x7f'
    assert math.isinf(codec.decode(b'\0\0\0\0\0\0\xf0\x7f'))
    assert codec.encode(float('nan')) == b'\0\0\0\0\0\0\xf8\x7f'
    assert math.isnan(codec.decode(b'\0\0\0\0\0\0\xf8\x7f'))

    # implicit conversion to float
    assert codec.encode(1) == b'\0\0\0\0\0\0\xf0?'

    # bad inputs
    with pytest.raises(struct.error) as e:
        assert codec.encode("test")
    e.match('required argument is not a float')

    with pytest.raises(struct.error) as e:
        assert codec.decode(b'\0')
    e.match('unpack requires a string argument of length 8')


def test_rgbimage_attr():
    codec = opendiamond.attributes.RGBImageAttributeCodec()

    # RGB image
    image = PIL.Image.new('RGB', (1, 2))
    rawimage = b'\0\0\0\0\x18\0\0\0\2\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0'
    assert codec.encode(image) == rawimage
    assert codec.decode(rawimage) == image

    with pytest.raises(SystemError):
        # 0x0 pixel RGB image
        image = PIL.Image.new('RGB', (0, 0))
        assert codec.encode(image)

    for format in ('RGBA', 'L', '1'):
        with pytest.raises(ValueError) as e:
            image = PIL.Image.new(format, (1, 1))
            assert codec.encode(image)
        e.match('No packer found from %s to RGBX' % format)

    # truncated header
    with pytest.raises(struct.error) as e:
        assert codec.decode(b'')
    e.match('unpack requires a string argument of length 8')

    with pytest.raises(struct.error) as e:
        assert codec.decode(rawimage[:15])
    e.match('unpack requires a string argument of length 8')

    # truncated image data
    with pytest.raises(ValueError) as e:
        assert codec.decode(rawimage[:16])
    e.match('not enough image data')

    # extra data in the raw image gets ignored
    badimage = rawimage[:8] + b'\1' + rawimage[9:]
    assert codec.decode(badimage).size == (1, 1)

    # insufficient data
    with pytest.raises(ValueError) as e:
        badimage = rawimage[:8] + b'\3' + rawimage[9:]
        assert codec.decode(badimage)
    e.match('not enough image data')


def test_patches_attr():
    codec = opendiamond.attributes.PatchesAttributeCodec()

    nopatches = b'\0\0\0\0\0\0\0\0\0\0\xf0?'
    assert codec.encode((1.0, [])) == nopatches
    assert codec.decode(nopatches) == (1.0, ())

    onepatch = b'\1\0\0\0\0\0\0\0\0\0\xf0?\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'
    assert codec.encode((1.0, [((0, 0), (0, 0))])) == onepatch
    assert codec.decode(onepatch) == (1.0, (((0, 0), (0, 0)),))

    # implicit conversions
    assert codec.encode((1, [])) == nopatches
    assert codec.encode((1.0, ())) == nopatches

    # clipped patch data
    with pytest.raises(struct.error) as e:
        assert codec.decode(onepatch[:-1])
    e.match('unpack requires a string argument of length 16')

    # insufficient number of patches
    with pytest.raises(struct.error):
        badpatch = b'\2' + onepatch[1:]
        assert codec.decode(badpatch)
    e.match('unpack requires a string argument of length 16')


def test_heatmap_attr():
    codec = opendiamond.attributes.HeatMapAttributeCodec()

    heatmap = PIL.Image.new('L', (1, 1))
    raw_heatmap = (
        b"\x89PNG\r\n\x1a\n\0\0\0\rIHDR\0\0\0\1\0\0\0\1\x08\0\0\0\0:~\x9b" +
        b"U\0\0\0\nIDATx\xdac`\0\0\0\2\0\1\xe5'\xde\xfc\0\0\0\0IEND\xaeB`\x82")
    assert codec.encode(heatmap) == raw_heatmap

    # cannot compare directly because decoded image is 'PIL.PngImagePlugin'
    # and heatmap is PIL.Image.Image
    assert codec.decode(raw_heatmap).format == 'PNG'
    assert codec.decode(raw_heatmap).mode == heatmap.mode
    assert codec.decode(raw_heatmap).size == heatmap.size
    assert codec.decode(raw_heatmap).load()[0, 0] == heatmap.load()[0, 0]

    # heatmaps are always converted to greyscale during encoding
    heatmap = PIL.Image.new('RGB', (1, 1))
    assert codec.encode(heatmap) == raw_heatmap
