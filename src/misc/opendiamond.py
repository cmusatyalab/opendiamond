#!/usr/bin/python

from ctypes import *
import UserDict

class odiskobj(UserDict.DictMixin):
    def __init__(self):
        opendiamond = CDLL("libopendiamond.so.3", mode=RTLD_GLOBAL)

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
	data = str(value)
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

class rgbimg_filter():
    def __init__(self):
        filter = CDLL("/home/jaharkes/git/snapfind/src/target/lib/fil_rgb.so")

        self._f_init = filter.f_init_img2rgb
        self._f_init.argtypes = [c_int, c_void_p, c_int, c_void_p, c_char_p, c_void_p]
        self._f_init.restype = c_int

        self._f_fini = filter.f_fini_img2rgb
        self._f_fini.argtypes = [c_void_p]
        self._f_fini.restype = c_int

        self._f_eval = filter.f_eval_img2rgb
        self._f_eval.argtypes = [c_void_p, c_void_p]
        self._f_eval.restype = c_int

	self._data = c_void_p()
	self._f_init(0, None, 0, None, "RGBIMG", byref(self._data))

    def __del__(self):
	self._f_fini(self._data)

    def __call__(self, obj):
	self._f_eval(obj._obj, self._data)

if __name__ == '__main__':
    o = odiskobj()
    o['a'] = 12
    o['b'] = 'hello\0'
    print o

    o[''] = open("img.jpg").read()

    filter = rgbimg_filter()
    filter(o)

    print o 
