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

'''XDR encoding helpers.'''

from builtins import zip
from builtins import object
import struct
from xdrlib import Packer, Unpacker, Error as XDRError


class XDREncodingError(Exception):
    pass


class _XDRTypeHandler(object):
    def pack(self, xdr, val):
        '''Serialize the object into an XDR stream.'''
        raise NotImplementedError()

    def unpack(self, xdr):
        '''Deserialize the object from an XDR stream and return it.'''
        raise NotImplementedError()


class _XDRPrimitiveHandler(_XDRTypeHandler):
    def __init__(self, name):
        _XDRTypeHandler.__init__(self)
        self._name = name

    def pack(self, xdr, val):
        getattr(xdr, 'pack_' + self._name)(val)

    def unpack(self, xdr):
        return getattr(xdr, 'unpack_' + self._name)()


class _XDRStringHandler(_XDRTypeHandler):
    # Due to https://bugs.python.org/issue9544
    # xdrlib in Python 3 doesn't pack string directly, can only pack bytes
    def __init__(self, encoding="utf-8", errors="strict"):
        _XDRTypeHandler.__init__(self)
        self.encoding = encoding
        self.errors = errors

    def pack(self, xdr, val):
        b = val.encode(self.encoding, self.errors)
        return xdr.pack_opaque(b)

    def unpack(self, xdr):
        b = xdr.unpack_opaque()
        return b.decode(self.encoding, self.errors)


class _XDRFOpaqueHandler(_XDRTypeHandler):
    def __init__(self, length):
        _XDRTypeHandler.__init__(self)
        self._length = length

    def _check(self, val):
        if len(val) != self._length:
            raise XDREncodingError()
        return val

    def pack(self, xdr, val):
        xdr.pack_fopaque(self._length, self._check(val))

    def unpack(self, xdr):
        return self._check(xdr.unpack_fopaque(self._length))


class _XDRArrayHandler(_XDRTypeHandler):
    def __init__(self, item_handler):
        _XDRTypeHandler.__init__(self)
        self._item_handler = item_handler

    def pack(self, xdr, val):
        # Packer.pack_array() is inconvenient for recursive descent.
        xdr.pack_uint(len(val))
        for item in val:
            self._item_handler.pack(xdr, item)

    def unpack(self, xdr):
        return xdr.unpack_array(lambda: self._item_handler.unpack(xdr))


class _XDROptionalHandler(_XDRTypeHandler):
    def __init__(self, item_handler):
        _XDRTypeHandler.__init__(self)
        self._item_handler = item_handler

    def pack(self, xdr, val):
        if val is not None:
            xdr.pack_bool(True)
            self._item_handler.pack(xdr, val)
        else:
            xdr.pack_bool(False)

    def unpack(self, xdr):
        if xdr.unpack_bool():
            return self._item_handler.unpack(xdr)
        return None


class _XDRConstantHandler(_XDRTypeHandler):
    def __init__(self, item_handler, value):
        _XDRTypeHandler.__init__(self)
        self._item_handler = item_handler
        self._value = value

    def pack(self, xdr, _val):
        self._item_handler.pack(xdr, self._value)

    def unpack(self, xdr):
        self._item_handler.unpack(xdr)
        return self._value


class _XDRStructHandler(_XDRTypeHandler):
    def __init__(self, struct_class):
        _XDRTypeHandler.__init__(self)
        self._struct_class = struct_class

    def pack(self, xdr, val):
        if not isinstance(val, self._struct_class):
            raise XDREncodingError()
        val.encode_xdr(xdr)

    def unpack(self, xdr):
        return self._struct_class.decode_xdr(xdr)


class XDR(object):
    '''Class containing static factory functions for XDR type handlers.'''
    def __init__(self):
        raise RuntimeError('Class cannot be instantiated')

    @staticmethod
    def int():
        return _XDRPrimitiveHandler('int')

    @staticmethod
    def uint():
        return _XDRPrimitiveHandler('uint')

    @staticmethod
    def hyper():
        return _XDRPrimitiveHandler('hyper')

    @staticmethod
    def double():
        return _XDRPrimitiveHandler('double')

    @staticmethod
    def string():
        return _XDRStringHandler()

    @staticmethod
    def opaque():
        return _XDRPrimitiveHandler('opaque')

    @staticmethod
    def fopaque(length):
        return _XDRFOpaqueHandler(length)

    @staticmethod
    def array(item_handler):
        return _XDRArrayHandler(item_handler)

    @staticmethod
    def optional(item_handler):
        return _XDROptionalHandler(item_handler)

    @staticmethod
    def constant(item_handler, value):
        return _XDRConstantHandler(item_handler, value)

    @staticmethod
    def struct(struct_class):
        return _XDRStructHandler(struct_class)


# This is a context manager, we're not using naming rules for classes
# pylint: disable=invalid-name
class _convert_exceptions(object):
    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if type in (XDRError, ValueError, struct.error):
            raise XDREncodingError()
        return False
# pylint: enable=invalid-name


class XDRStruct(object):
    '''Base class for an XDR struct.'''

    members = ()

    def __init__(self, *args, **kwargs):
        attrs = [a for a, _h in self._members() if a is not None]
        for attr in attrs:
            setattr(self, attr, None)
        for attr, arg in zip(attrs, args):
            setattr(self, attr, arg)
        for k, v in kwargs.items():
            if k not in attrs:
                raise TypeError('No such keyword argument: %s' % k)
            setattr(self, k, v)

    @classmethod
    def _members(cls):
        '''cls.members, converted into a list of 2-tuples (attr, handler).'''
        return list(zip(cls.members[::2], cls.members[1::2]))

    def encode(self):
        '''Return the serialized bytes for the object.'''
        with _convert_exceptions():
            xdr = Packer()
            self.encode_xdr(xdr)
            return xdr.get_buffer()

    @classmethod
    def decode(cls, data):
        '''Deserialize the data and return an object.'''
        with _convert_exceptions():
            xdr = Unpacker(data)
            ret = cls.decode_xdr(xdr)
            xdr.done()
            return ret

    def encode_xdr(self, xdr):
        '''Serialize the object into an XDR stream.'''
        for attr, handler in self._members():
            if attr is not None:
                value = getattr(self, attr)
            else:
                value = None
            try:
                handler.pack(xdr, value)
            except Exception as e:
                raise XDREncodingError('Failed to encode attr {}'.format(attr)) from e

    @classmethod
    def decode_xdr(cls, xdr):
        '''Deserialize the object from an XDR stream.'''
        kwargs = {}
        for attr, handler in cls._members():
            value = handler.unpack(xdr)
            if attr is not None:
                kwargs[attr] = value
        return cls(**kwargs)
