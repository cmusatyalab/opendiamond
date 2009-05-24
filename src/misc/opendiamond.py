#!/usr/bin/python

from ctypes import *
import UserDict
import os

libopendiamond = CDLL("libopendiamond.so.3", mode=RTLD_GLOBAL)

odisk_null_obj = CFUNCTYPE(c_void_p) \
	(('odisk_null_obj', libopendiamond))
odisk_release_obj = CFUNCTYPE(c_int, c_void_p) \
	(('odisk_release_obj', libopendiamond), ((1, "object"),))

lf_write_attr = CFUNCTYPE(c_int, c_void_p, c_char_p, c_int, c_void_p) \
	(('lf_write_attr', libopendiamond),
	 ((1, "object"), (1, "name"), (1, "len"), (1, "data")))
lf_ref_attr = CFUNCTYPE(c_int, c_void_p, c_char_p, POINTER(c_int),
			POINTER(c_void_p)) \
	(('lf_ref_attr', libopendiamond),
	 ((1, "object"), (1, "name"), (2, "len"), (2, "data")))

def _attr_errcheck(result, func, args):
    if result: raise KeyError
    return args
lf_write_attr.errcheck = _attr_errcheck
lf_ref_attr.errcheck = _attr_errcheck

lf_first_attr = CFUNCTYPE(c_int, c_void_p, POINTER(c_char_p), POINTER(c_int),
			  POINTER(c_void_p), POINTER(c_void_p)) \
	(('lf_first_attr', libopendiamond),
	 ((1, "object"), (2, "name"), (2, "len"), (2, "data"), (1, "cookie")))
lf_next_attr = CFUNCTYPE(c_int, c_void_p, POINTER(c_char_p), POINTER(c_int),
			 POINTER(c_void_p), POINTER(c_void_p)) \
	(('lf_next_attr', libopendiamond),
	 ((1, "object"), (2, "name"), (2, "len"), (2, "data"), (1, "cookie")))

def _iter_errcheck(result, func, args):
    if result: raise StopIteration
    return args
lf_first_attr.errcheck = _iter_errcheck
lf_next_attr.errcheck = _iter_errcheck

class odiskobj(UserDict.DictMixin):
    def __init__(self):
        self._as_parameter_ = odisk_null_obj()

    @classmethod
    def from_param(cls, obj):
	if not isinstance(obj, cls):
	    raise TypeError, "expected odiskobj as parameter"
	return obj._as_parameter_

    def __del__(self):
        try:
            odisk_release_obj(self)
        except:
            pass

    def __getitem__(self, key):
	len, data = lf_ref_attr(self, key)
        value = (c_char * len.value)()
        memmove(value, data, len.value)
        return value.raw

    def __setitem__(self, key, value):
	data = str(value)
        lf_write_attr(self, key, len(data), data)

    def iteritems(self):
	cookie = c_void_p()
	name, len, data = lf_first_attr(self, byref(cookie))
	while 1:
	    value = (c_char * len)()
	    memmove(value, data, len)
	    yield name, value.raw
	    name, len, data = lf_next_attr(self, byref(cookie))

    def __iter__(self):
	for k, _ in self.iteritems():
	    yield k

    def keys(self):
	return list(self.__iter__())

    def has_key(self, key):
	try: lf_ref_attr(self, key)
	except KeyError: return False
	return True


PROTO_FILTER_INIT = CFUNCTYPE(c_int, c_int, POINTER(c_char_p), c_int, c_void_p, c_char_p, POINTER(c_void_p))
param_filter_init = (1, "num_args", 0), (1, "args", None), (1, "bloblen", 0), (1, "blob_data", None), (1, "name"), (2, "filter_args")

PROTO_FILTER_EVAL = CFUNCTYPE(c_int, odiskobj, c_void_p)
param_filter_eval = (1, "object"), (1, "filter_args")

PROTO_FILTER_FINI = CFUNCTYPE(c_int, c_void_p)
param_filter_fini = (1, "filter_args"),

class Filter(object):
    def __init__(self, filter, eval, init=None, fini=None, *args):
	self._fini = self._data = None
        so = CDLL(filter)
        self._eval = PROTO_FILTER_EVAL((eval, so), param_filter_eval)
	if init:
	    init = PROTO_FILTER_INIT((init, so), param_filter_init)
	    self._fini = PROTO_FILTER_FINI((fini, so), param_filter_fini)

	    ARGARRAY = c_char_p * len(args)
	    self._data = init(num_args=len(args), args=ARGARRAY(*args),
			      name=os.path.basename(filter))

    def __call__(self, obj):
	self._eval(obj, self._data)

    def __del__(self):
	if self._fini: self._fini(self._data)

if __name__ == '__main__':
    rgbimg = Filter(filter="./fil_rgb.so", eval="f_eval_img2rgb",
		    init="f_init_img2rgb", fini="f_fini_img2rgb")

    o = odiskobj()
    o[''] = open("img.jpg").read()

    rgbimg(o)
    print o.keys()

