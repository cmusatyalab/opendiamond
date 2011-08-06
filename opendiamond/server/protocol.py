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

'''XDR serialization/deserialization for the Diamond wire protocol.'''

# XDR classes are oddly named for consistency with OpenDiamond-Java
# pylint: disable=C0103

from opendiamond.server.rpc import XDREncodable, RPCError, RPCEncodingError

MAX_ATTRIBUTE_NAME = 256
MAX_FILTER_NAME = 128
MAX_FILTERS = 64
MAX_BLOBS = 2 * MAX_FILTERS	# Filter + blob argument

class DiamondRPCFailure(RPCError):
    '''Generic Diamond RPC failure.'''
    code = 500
class DiamondRPCFCacheMiss(RPCError):
    '''Filter code or blob argument missed in the blob cache.'''
    code = 501
class DiamondRPCCookieExpired(RPCError):
    '''Proffered scope cookie has expired.'''
    code = 504
class DiamondRPCSchemeNotSupported(RPCError):
    '''URI scheme not supported.'''
    code = 505


class XDR_attribute(XDREncodable):
    '''An object attribute; reply only'''
    def __init__(self, name, value):
        XDREncodable.__init__(self)
        self.name = name
        self.value = value

    def encode(self, xdr):
        if len(self.name) > MAX_ATTRIBUTE_NAME:
            raise RPCEncodingError()
        xdr.pack_string(self.name)
        xdr.pack_opaque(self.value)


class XDR_object(XDREncodable):
    '''Blast channel object data; reply only'''
    def __init__(self, search_id, obj, attrs):
        '''attrs is a list of XDR_attribute.'''
        XDREncodable.__init__(self)
        self.search_id = search_id
        self.obj = obj
        self.attrs = attrs

    def encode(self, xdr):
        xdr.pack_uint(self.search_id)
        xdr.pack_opaque(self.obj)
        self.encode_array(xdr, self.attrs)


class XDR_blob_list(XDREncodable):
    '''A list of blob URIs; reply only'''
    def __init__(self, uris):
        '''uris is a list of strings.'''
        XDREncodable.__init__(self)
        self.uris = uris

    def encode(self, xdr):
        if len(self.uris) > MAX_BLOBS:
            raise RPCEncodingError()
        xdr.pack_array(self.uris, xdr.pack_string)


class XDR_filter_config(object):
    '''Configuration for a single filter; request only'''
    def __init__(self, xdr):
        self.name = xdr.unpack_string()
        self.arguments = xdr.unpack_array(xdr.unpack_string)
        self.dependencies = xdr.unpack_array(xdr.unpack_string)
        self.min_score = xdr.unpack_double()
        self.max_score = xdr.unpack_double()
        self.code = xdr.unpack_string()
        self.blob = xdr.unpack_string()
        if (len(self.name) > MAX_FILTER_NAME or
                        len(self.dependencies) > MAX_FILTERS):
            raise RPCEncodingError()
        for name in self.dependencies:
            if len(name) > MAX_FILTER_NAME:
                raise RPCEncodingError()


class XDR_setup(object):
    '''Search setup parameters; request only'''
    def __init__(self, xdr):
        self.cookies = xdr.unpack_array(xdr.unpack_string)
        self.filters = xdr.unpack_array(lambda: XDR_filter_config(xdr=xdr))
        if len(self.filters) > MAX_FILTERS:
            raise RPCEncodingError()


class XDR_blob_data(object):
    '''Blob data to be added to the blob cache; request only'''
    def __init__(self, xdr):
        self.blobs = xdr.unpack_array(xdr.unpack_opaque)
        if len(self.blobs) > MAX_BLOBS:
            raise RPCEncodingError()


class XDR_start(object):
    '''Start-search parameters; request only'''
    def __init__(self, xdr):
        self.search_id = xdr.unpack_uint()
        if xdr.unpack_bool():
            self.attrs = xdr.unpack_array(xdr.unpack_string)
            for attr in self.attrs:
                if len(attr) > MAX_ATTRIBUTE_NAME:
                    raise RPCEncodingError()
        else:
            self.attrs = None


class _XDRStats(XDREncodable):
    '''Base class for XDR_filter_stats and XDR_search_stats.'''

    stats = ()
    hyper_stats = set()

    def __init__(self, **kwargs):
        XDREncodable.__init__(self)
        for stat in self.stats:
            setattr(self, stat, kwargs.get(stat, 0))

    def encode(self, xdr):
        for stat in self.stats:
            if stat in self.hyper_stats:
                xdr.pack_hyper(getattr(self, stat))
            else:
                xdr.pack_int(getattr(self, stat))


class XDR_filter_stats(_XDRStats):
    '''Filter statistics; reply only'''

    stats = ('objs_processed', 'objs_dropped',
            'objs_cache_dropped', 'objs_cache_passed', 'objs_compute',
            # hits_* is unused
            'hits_inter_session', 'hits_inter_query', 'hits_intra_query',
            'avg_exec_time')
    hyper_stats = set(['avg_exec_time'])

    def __init__(self, name, **kwargs):
        _XDRStats.__init__(self, **kwargs)
        self.name = name

    def encode(self, xdr):
        if len(self.name) > MAX_FILTER_NAME:
            raise RPCEncodingError()
        xdr.pack_string(self.name)
        _XDRStats.encode(self, xdr)


class XDR_search_stats(_XDRStats):
    '''Search statistics; reply only'''

    # system_load is unused, and objs_nproc should be called objs_passed
    stats = ('objs_total', 'objs_processed', 'objs_dropped', 'objs_nproc',
            'system_load', 'avg_obj_time')
    hyper_stats = set(['avg_obj_time'])

    def __init__(self, filter_stats, **kwargs):
        '''filter_stats is a list of XDR_filter_stats.'''
        _XDRStats.__init__(self, **kwargs)
        self.filter_stats = filter_stats

    def encode(self, xdr):
        if len(self.filter_stats) > MAX_FILTERS:
            raise RPCEncodingError()
        _XDRStats.encode(self, xdr)
        self.encode_array(xdr, self.filter_stats)


class XDR_session_var(XDREncodable):
    '''Session variable'''
    def __init__(self, xdr=None, name=None, value=None):
        XDREncodable.__init__(self)
        if xdr is not None:
            self.name = xdr.unpack_string()
            self.value = xdr.unpack_double()
        else:
            self.name = name
            self.value = value

    def encode(self, xdr):
        xdr.pack_string(self.name)
        xdr.pack_double(self.value)


class XDR_session_vars(XDREncodable):
    '''Session variable list'''
    def __init__(self, xdr=None, vars=None):
        '''vars is a list of XDR_session_var.'''
        XDREncodable.__init__(self)
        if xdr is not None:
            self.vars = xdr.unpack_array(lambda: XDR_session_var(xdr=xdr))
        else:
            self.vars = vars

    def encode(self, xdr):
        self.encode_array(xdr, self.vars)


class XDR_reexecute(object):
    '''Reexecute argument; request only'''
    def __init__(self, xdr):
        self.object_id = xdr.unpack_string()
        self.attrs = xdr.unpack_array(xdr.unpack_string)
        for attr in self.attrs:
            if len(attr) > MAX_ATTRIBUTE_NAME:
                raise RPCEncodingError()


class XDR_attribute_list(XDREncodable):
    '''Reexecute response; reply only'''
    def __init__(self, attrs):
        '''attrs is a list of XDR_attribute.'''
        XDREncodable.__init__(self)
        self.attrs = attrs

    def encode(self, xdr):
        self.encode_array(xdr, self.attrs)
