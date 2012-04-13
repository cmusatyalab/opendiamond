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

'''XDR serialization/deserialization for the Diamond wire protocol.'''

# XDR classes are oddly named for consistency with OpenDiamond-Java
# pylint: disable=C0103

from opendiamond.rpc import RPCError
from opendiamond.xdr import XDR, XDRStruct

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


class XDR_attribute(XDRStruct):
    '''An object attribute'''
    members = (
        'name', XDR.string(MAX_ATTRIBUTE_NAME),
        'value', XDR.opaque(),
    )


class XDR_object(XDRStruct):
    '''Blast channel object data'''
    members = (
        'search_id', XDR.uint(),
        # was object data, now stored in attrs['']
        None, XDR.constant(XDR.opaque(), ''),
        'attrs', XDR.array(XDR.struct(XDR_attribute)),
    )


class XDR_blob_list(XDRStruct):
    '''A list of blob URIs'''
    members = (
        'uris', XDR.array(XDR.string(), MAX_BLOBS),
    )


class XDR_filter_config(XDRStruct):
    '''Configuration for a single filter'''
    members = (
        'name', XDR.string(MAX_FILTER_NAME),
        'arguments', XDR.array(XDR.string()),
        'dependencies', XDR.array(XDR.string(MAX_FILTER_NAME), MAX_FILTERS),
        'min_score', XDR.double(),
        'max_score', XDR.double(),
        'code', XDR.string(),
        'blob', XDR.string(),
    )


class XDR_setup(XDRStruct):
    '''Search setup parameters'''
    members = (
        'cookies', XDR.array(XDR.string()),
        'filters', XDR.array(XDR.struct(XDR_filter_config), MAX_FILTERS),
    )


class XDR_blob_data(XDRStruct):
    '''Blob data to be added to the blob cache'''
    members = (
        'blobs', XDR.array(XDR.opaque(), MAX_BLOBS),
    )


class XDR_start(XDRStruct):
    '''Start-search parameters'''
    members = (
        'search_id', XDR.uint(),
        'attrs', XDR.optional(XDR.array(XDR.string(MAX_ATTRIBUTE_NAME))),
    )


class _XDRStats(XDRStruct):
    '''Base class for XDR_filter_stats and XDR_search_stats'''

    def __init__(self, *args, **kwargs):
        XDRStruct.__init__(self, *args, **kwargs)
        for attr, _handler in self._members():
            if attr is not None and getattr(self, attr) is None:
                setattr(self, attr, 0)


class XDR_filter_stats(_XDRStats):
    '''Filter statistics'''
    members = (
        'name', XDR.string(MAX_FILTER_NAME),
        'objs_processed', XDR.int(),
        'objs_dropped', XDR.int(),
        'objs_cache_dropped', XDR.int(),
        'objs_cache_passed', XDR.int(),
        'objs_compute', XDR.int(),
        None, XDR.constant(XDR.int(), 0),  # hits_inter_session
        None, XDR.constant(XDR.int(), 0),  # hits_inter_query
        None, XDR.constant(XDR.int(), 0),  # hits_intra_query
        'avg_exec_time', XDR.hyper(),
    )


class XDR_search_stats(_XDRStats):
    '''Search statistics'''
    members = (
        'objs_total', XDR.int(),
        'objs_processed', XDR.int(),
        'objs_dropped', XDR.int(),
        'objs_nproc', XDR.int(),  # should be called objs_passed
        None, XDR.constant(XDR.int(), 0),  # system_load
        'avg_obj_time', XDR.hyper(),
        'filter_stats', XDR.array(XDR.struct(XDR_filter_stats), MAX_FILTERS),
    )


class XDR_session_var(XDRStruct):
    '''Session variable'''
    members = (
        'name', XDR.string(),
        'value', XDR.double(),
    )


class XDR_session_vars(XDRStruct):
    '''Session variable list'''
    members = (
        'vars', XDR.array(XDR.struct(XDR_session_var)),
    )


class XDR_reexecute(XDRStruct):
    '''Reexecute argument'''
    members = (
        'object_id', XDR.string(),
        'attrs', XDR.array(XDR.string(MAX_ATTRIBUTE_NAME)),
    )


class XDR_attribute_list(XDRStruct):
    '''Reexecute response'''
    members = (
        'attrs', XDR.array(XDR.struct(XDR_attribute)),
    )
