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
# pylint: disable=invalid-name

from opendiamond.rpc import RPCError
from opendiamond.xdr import XDR, XDRStruct

# Default port
PORT = 5872
# Nonce details
NONCE_LEN = 16
NULL_NONCE = b'\x00' * NONCE_LEN


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
        'name', XDR.string(),
        'value', XDR.opaque(),
    )


class XDR_object(XDRStruct):
    '''Blast channel object data'''
    members = (
        'attrs', XDR.array(XDR.struct(XDR_attribute)),
    )


class XDR_blob_list(XDRStruct):
    '''A list of blob URIs'''
    members = (
        'uris', XDR.array(XDR.string()),
    )


class XDR_filter_config(XDRStruct):
    '''Configuration for a single filter'''
    members = (
        'name', XDR.string(),
        'arguments', XDR.array(XDR.string()),
        'dependencies', XDR.array(XDR.string()),
        'min_score', XDR.double(),
        'max_score', XDR.double(),
        'code', XDR.string(),
        'blob', XDR.string(),
    )


class XDR_setup(XDRStruct):
    '''Search setup parameters'''
    members = (
        'cookies', XDR.array(XDR.string()),
        'filters', XDR.array(XDR.struct(XDR_filter_config)),
    )


class XDR_blob_data(XDRStruct):
    '''Blob data to be added to the blob cache'''
    members = (
        'blobs', XDR.array(XDR.opaque()),
    )


class XDR_start(XDRStruct):
    '''Start-search parameters'''
    members = (
        'search_id', XDR.fopaque(36),
        'attrs', XDR.optional(XDR.array(XDR.string())),
    )


class XDR_stat(XDRStruct):
    '''Statistics key-value pair'''
    members = (
        "name", XDR.string(),
        "value", XDR.hyper(),
    )


class XDR_filter_stats(XDRStruct):
    '''Filter statistics'''
    members = (
        'name', XDR.string(),
        'stats', XDR.array(XDR.struct(XDR_stat)),
    )


class XDR_search_stats(XDRStruct):
    '''Search statistics'''
    members = (
        'stats', XDR.array(XDR.struct(XDR_stat)),
        'filter_stats', XDR.optional(XDR.array(XDR.struct(XDR_filter_stats))),
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
        'attrs', XDR.optional(XDR.array(XDR.string())),
    )

class XDR_retrain(XDRStruct):
    '''Search retrain parameters'''
    members = (
        'names', XDR.array(XDR.string()),
        'labels', XDR.array(XDR.int()),
        'features', XDR.array(XDR.opaque()),
    )

class XDR_attribute_list(XDRStruct):
    '''Reexecute response'''
    members = (
        'attrs', XDR.array(XDR.struct(XDR_attribute)),
    )
