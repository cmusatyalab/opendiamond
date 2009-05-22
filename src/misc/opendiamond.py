#!/usr/bin/python

from ctypes import *
import UserDict

class odiskobj(UserDict.DictMixin):
    def __init__(self):
        opendiamond = CDLL("libopendiamond.so.3")

        self._odisk_null_obj = opendiamond.odisk_null_obj
        self._odisk_null_obj.argtypes = []
        self._odisk_null_obj.restype = c_void_p

        self._odisk_release_obj = opendiamond.odisk_release_obj
        self._odisk_release_obj.argtypes = [c_void_p]
        self._odisk_release_obj.restype = c_int

        self._lf_write_attr = opendiamond.lf_write_attr
        self._lf_write_attr.restype = c_int
        self._lf_write_attr.argtypes = [c_void_p, c_char_p, c_int, c_void_p]

        self._lf_ref_attr = opendiamond.lf_ref_attr
        self._lf_ref_attr.restype = c_int
        self._lf_ref_attr.argtypes = [c_void_p, c_char_p, c_void_p, c_void_p]

        self._lf_first_attr = opendiamond.lf_first_attr
        self._lf_first_attr.restype = c_int
        self._lf_first_attr.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_void_p]

        self._lf_next_attr = opendiamond.lf_next_attr
        self._lf_next_attr.restype = c_int
        self._lf_next_attr.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_void_p]

        self._obj = self._odisk_null_obj()

    def __del__(self):
        try:
            self._odisk_release_obj(self._obj)
        except:
            pass

    def __getitem__(self, key):
        len = c_int()
        data = c_void_p()
        err = self._lf_ref_attr(self._obj, key, byref(len), byref(data))
        value = (c_char * len.value)()
        memmove(value, data, len.value)
        return value.raw

    def __setitem__(self, key, value):
        data = bytes(value)
        self._lf_write_attr(self._obj, key, len(data), data)

    def keys(self):
        result = []

        name = c_char_p()
        len = c_int()
        data = c_char_p()
        cookie = c_void_p()
        err = self._lf_first_attr(self._obj, byref(name),
                                  byref(len), byref(data),
                                  byref(cookie))
        while err == 0:
            result.append(name.value)
            err = self._lf_next_attr(self._obj, byref(name),
                                     byref(len), byref(data),
                                     byref(cookie))
        return result


if __name__ == '__main__':
    o = odiskobj()
    o['a'] = 12
    o['b'] = 'hello\0'
    print o
