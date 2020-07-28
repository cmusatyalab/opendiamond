#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Representations of a Diamond object.'''
from __future__ import print_function

from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import object
from io import BytesIO
import logging
from urllib.parse import urljoin
import simplejson as json

import pycurl as curl

import xmltodict

from opendiamond.helpers import murmur, split_scheme
from opendiamond.protocol import XDR_attribute, XDR_object

ATTR_HEADER_URL = 'x-attributes'
ATTR_HEADER_PREFIX = 'x-attr-'
ATTR_HEADER_PREFIX_LEN = len(ATTR_HEADER_PREFIX)
# Object attributes handled directly by the server
ATTR_DATA = ''
ATTR_OBJ_ID = '_ObjectID'
ATTR_DISPLAY_NAME = 'Display-Name'
ATTR_DEVICE_NAME = 'Device-Name'

# Initialize curl before multiple threads have been started
curl.global_init(curl.GLOBAL_DEFAULT)

_log = logging.getLogger(__name__)


class ObjectLoadError(Exception):
    '''Object failed to load.'''


class EmptyObject(object):
    '''An immutable Diamond object with no data and no attributes.'''

    def __init__(self):
        self._attrs = dict()
        self._signatures = dict()
        self._omit_attrs = set()

    def __str__(self):
        return ''

    def __repr__(self):
        return '<EmptyObject>'

    def __iter__(self):
        '''Return an iterator over the attribute names.'''
        return iter(self._attrs.keys())

    def __contains__(self, key):
        return key in self._attrs

    def __getitem__(self, key):
        return self._attrs[key]

    def __setitem__(self, key, value):
        raise TypeError()

    def get_signature(self, key):
        '''Return the MD5 hash of the attribute value.'''
        return self._signatures[key]

    def omit(self, key):
        '''Record that the attribute is not to be returned to the client.'''
        if key in self:
            self._omit_attrs.add(key)
        else:
            raise KeyError()

    def xdr_attributes(self, output_set=None, for_drop=False):
        '''Return a list of XDR_attribute.'''
        if for_drop:
            # Reexecution dropped the object.  Only encode the object ID.
            send_keys = set([ATTR_OBJ_ID])
        else:
            # Don't encode any evidence of omit attributes.
            send_keys = set(self._attrs.keys()) - self._omit_attrs
        # If we have an output set, only send values for send_keys that are
        # in it.  Otherwise, send values for all send_keys.
        if output_set is not None:
            # Ensure the object ID is always sent.  Don't modify the output
            # set in place.
            output_set = output_set.union([ATTR_OBJ_ID])
            send_values = send_keys.intersection(output_set)
        else:
            send_values = send_keys
        _log.debug("{}: Sending attrs: {}".format(str(self), ','.join(send_values)))
        # Serialize
        attrs = []
        for name in send_keys:
            if name in send_values:
                value = self._attrs[name]
                if not isinstance(value, bytes):
                    value = str(value).encode() # to bytes
            else:
                value = b''
            attrs.append(XDR_attribute(name, value))
        return attrs

    def xdr(self, output_set=None):
        '''Return an XDR_object.'''
        return XDR_object(self.xdr_attributes(output_set))

    def debug(self):
        """Print all attributes"""
        print(str(self))
        print('<internal> %s: %s' % ('src', getattr(self, 'src', None)))
        print('<internal> %s: %s' % ('meta', getattr(self, 'meta', None)))
        for k, v in self._attrs.items():
            if k:   # Skip data attribute
                print('%s: %s' % (k, v))


class Object(EmptyObject):
    '''A mutable Diamond object.'''

    def __init__(self, server_id, url, src=None, meta=None, compute_signature=True):
        EmptyObject.__init__(self)
        self._id = url
        self._compute_signature = compute_signature

        if src:
            self.src = src
        if meta:
            self.meta = meta

        # Set default attributes
        self[ATTR_DEVICE_NAME] = server_id + '\0'
        self[ATTR_OBJ_ID] = url + '\0'

    def __str__(self):
        '''Return the object ID.'''
        return self._id

    def __repr__(self):
        return '<Object %s>' % self

    def __setitem__(self, key, value):
        self._attrs[key] = value
        if self._compute_signature:
            self._signatures[key] = murmur(value)
        else:
            self._signatures[key] = 0


class _HttpLoader(object):
    '''A context for loading Object data via HTTP.  Caches and reuses HTTP
    connections.  Must not be used by more than one thread.'''

    def __init__(self, config):
        self._curl = curl.Curl()
        self._curl.setopt(curl.NOSIGNAL, 1)
        self._curl.setopt(curl.FAILONERROR, 1)
        self._curl.setopt(curl.USERAGENT, config.user_agent)
        if config.http_proxy is not None:
            self._curl.setopt(curl.PROXY, config.http_proxy)
        self._curl.setopt(curl.HEADERFUNCTION, self._handle_header)
        self._curl.setopt(curl.WRITEFUNCTION, self._handle_body)
        self._headers = {}
        self._body = BytesIO()

    def get(self, url):
        '''Fetch the specified URL and return (header_dict, body).'''
        # Perform the fetch
        self._curl.setopt(curl.URL, url)
        try:
            self._curl.perform()
        except curl.error as e:
            raise ObjectLoadError(str(e.args[0]) + ';' + e.args[1] + ';' + self._curl.errstr())
        # Localize fetched data and release this object's copy
        headers = self._headers
        self._headers = {}
        body = self._body.getvalue()
        self._body = BytesIO()
        return (headers, body)

    def _handle_header(self, hdr):
        hdr = hdr.decode()  # to str
        hdr = hdr.rstrip('\r\n')
        if hdr.startswith('HTTP/'):
            # New HTTP status line, discard existing headers
            self._headers = {}
        elif hdr != '':
            # This is simplistic.
            key, value = hdr.split(': ', 1)
            self._headers[key] = value.encode() # value to bytes

    def _handle_body(self, data):
        self._body.write(data)


class ObjectLoader(object):
    """A context for populating an Object from the dataretriever.  Allows
    network connections to be reused to fetch multiple objects.  Must not
    be used by more than one thread.

    Attributes are also loaded. There are several possible places attributes
    are loaded from. The precedence order is (later overwrites earlier):
    - 'meta' URI from scope list <object /> element, which can either be a URL or
    local file path.
    - HTTP headers starting with ATTR_HEADER_PREFIX='x-attr-', when retrieving
    the object itself. (happen only when retrieving from data retriever)
    - HTTP header ATTR_HEADER_URL='x-attributes', when retrieving the object itself.
    (happen only when retrieving from data retriever)
    """

    def __init__(self, config, blob_cache):
        self._http = _HttpLoader(config)
        self._blob_cache = blob_cache

    def source_available(self, obj):
        '''Examine the Object and return whether we think we will be able
        to load it, without actually doing so.  This can be used during
        reexecution to determine whether we should return
        DiamondRPCFCacheMiss to the client.'''
        scheme, path = split_scheme(str(obj))
        if scheme == 'sha256':
            return path in self._blob_cache
        # Assume we can always load other types of URLs
        return True

    def load(self, obj):
        '''Retrieve the Object and update it with the information we
        receive.'''

        # 0. Load src URI if not exist (mainly used for re-execution)
        self._load_src_maybe_meta(obj)

        # 1. Load object metadata from meta URI
        if hasattr(obj, 'meta'):
            uri = str(obj.meta)
            scheme, path = split_scheme(uri)
            if scheme == 'file':
                self._load_attrbutes_localfile(obj, path)
            else:  # assume http
                self._load_attributes(obj, uri)

        # 2. Load object content from src URI
        uri = str(obj.src)
        scheme, path = split_scheme(uri)
        if scheme == 'sha256':
            self._load_blobcache(obj, path)
        elif scheme == 'file':
            self._load_localfile(obj, path)
        else:  # assume http
            self._load_dataretriever(obj, uri)

        # Set display name if not already in initial attributes
        if ATTR_DISPLAY_NAME not in obj:
            obj[ATTR_DISPLAY_NAME] = str(obj) + '\0'

    def _load_blobcache(self, obj, signature):
        # Load the object data
        try:
            obj[ATTR_DATA] = self._blob_cache[signature]
        except KeyError:
            raise ObjectLoadError('Object not in cache')

    def _load_dataretriever(self, obj, url):
        headers, body = self._http.get(url)
        # Load the object data
        obj[ATTR_DATA] = body
        # Process loose initial attributes
        for key, value in headers.items():
            if key.lower().startswith(ATTR_HEADER_PREFIX):
                key = key[ATTR_HEADER_PREFIX_LEN:]
                obj[key] = value + '\0'
        # Fetch additional initial attributes if specified
        if ATTR_HEADER_URL in headers:
            attr_url = urljoin(url, headers[ATTR_HEADER_URL])
            self._load_attributes(obj, attr_url)

    def _load_localfile(self, obj, path):
        with open(path, 'rb') as f:
            obj[ATTR_DATA] = f.read()

    # The return type of json.loads() confuses pylint
    # pylint: disable=maybe-no-member
    def _load_attributes(self, obj, url):
        '''Load JSON-encoded attribute data from the specified URL.'''
        _headers, body = self._http.get(url)
        try:
            attrs = json.loads(body)
            if not isinstance(attrs, dict):
                raise ObjectLoadError("Failed to retrieve object attributes")
        except ValueError as e:
            raise ObjectLoadError(str(e))
        for k, v in attrs.items():
            obj[k] = str(v) + '\0'

    # pylint: enable=maybe-no-member

    def _load_attrbutes_localfile(self, obj, path):
        try:
            with open(path, 'r') as f:
                attrs = json.load(f)
                if not isinstance(attrs, dict):
                    raise ObjectLoadError(
                        "Failed to retrieve object attributes")
        except IOError as e:
            raise ObjectLoadError(str(e))
        except ValueError as e:
            raise ObjectLoadError(str(e))
        for k, v in attrs.items():
            obj[k] = str(v) + '\0'

    def _load_src_maybe_meta(self, obj):
        """
        Load the src and optional meta URIs from the object's id.
        :param obj:
        :return:
        """
        if not hasattr(obj, 'src'):
            uri = str(obj)
            scheme, path = split_scheme(uri)
            if scheme == 'sha256':
                # Blob has no meta URI
                obj.src = uri
            else:
                # Assume http
                _, body = self._http.get(uri)
                object_el = xmltodict.parse(body)
                obj.src = urljoin(uri, object_el['object']['@src'])
                if '@meta' in object_el['object']:
                    obj.meta = urljoin(uri, object_el['object']['@meta'])
        return
