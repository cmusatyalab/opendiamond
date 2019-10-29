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

from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import range
from builtins import object
from io import BytesIO
import struct

import PIL.Image


class _AttributeCodec(object):
    '''Base class for a Diamond attribute codec.'''

    def encode(self, item):
        raise NotImplementedError()

    def decode(self, data):
        raise NotImplementedError()


class StringAttributeCodec(_AttributeCodec):
    '''Codec for a null-terminated string.'''

    def encode(self, item):
        assert isinstance(item, str)
        return str.encode(item + '\0')

    def decode(self, data):
        data = data.decode()
        if data[-1] != '\0':
            raise ValueError('Attribute value is not null-terminated: {}'.format(str(data)))
        return data[:-1]


class IntegerAttributeCodec(_AttributeCodec):
    '''Codec for a 32-bit native-endian integer.'''

    def encode(self, item):
        assert isinstance(item, int)
        return struct.pack('i', item)

    def decode(self, data):
        return struct.unpack('i', data)[0]


class DoubleAttributeCodec(_AttributeCodec):
    '''Codec for a native-endian double.'''

    def encode(self, item):
        assert isinstance(item, float)
        return struct.pack('d', item)

    def decode(self, data):
        return struct.unpack('d', data)[0]


class RGBImageAttributeCodec(_AttributeCodec):
    '''Codec for an RGBImage structure.  Decodes to a PIL.Image.'''

    def encode(self, item):
        assert isinstance(item, PIL.Image.Image)
        pixels = item.tobytes('raw', 'RGBX', 0, 1)
        width, height = item.size
        header = struct.pack('IIii', 0, len(pixels) + 16, height, width)
        return header + pixels

    def decode(self, data):
        # Parse the dimensions out of the header
        height, width = struct.unpack('2i', data[8:16])
        return PIL.Image.frombytes('RGB', (width, height), data[16:], 'raw', 'RGBX', 0, 1)


class PatchesAttributeCodec(_AttributeCodec):
    '''Codec for a list of patches.  Decodes to (distance, patches), where
    distance is a double, patches is a tuple of (upper_left_coord,
    lower_right_coord) tuples, and a coordinate is an (x, y) tuple.'''

    def encode(self, item):
        distance, patches = item
        pieces = [struct.pack('<id', len(patches), distance)]
        for a, b in patches:
            pieces.append(struct.pack('<iiii', a[0], a[1], b[0], b[1]))
        return b''.join(pieces)

    def decode(self, data):
        data, count, distance = self._parse('<id', data)
        patches = []
        for _i in range(count):
            data, x0, y0, x1, y1 = self._parse('<iiii', data)
            patches.append(((x0, y0), (x1, y1)))
        return distance, tuple(patches)

    def _parse(self, fmt, data):
        len = struct.calcsize(fmt)
        data, remainder = data[0:len], data[len:]
        return (remainder,) + struct.unpack(fmt, data)


class HeatMapAttributeCodec(_AttributeCodec):
    '''Codec for a heat map image.  Decodes to a PIL.Image.'''

    def encode(self, item):
        if item.mode != 'L':
            item = item.convert('L')
        buf = BytesIO()
        item.save(buf, 'png', optimize=1)
        return buf.getvalue()

    def decode(self, data):
        return PIL.Image.open(BytesIO(data))
