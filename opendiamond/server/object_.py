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

'''Representations of a Diamond object.'''

from cStringIO import StringIO
import pycurl as curl
from urlparse import urljoin
import simplejson as json

from opendiamond.helpers import md5, split_scheme
from opendiamond.protocol import XDR_attribute, XDR_object

ATTR_HEADER_URL = 'x-attributes'
ATTR_HEADER_PREFIX = 'x-attr-'
# Object attributes handled directly by the server
ATTR_DATA = ''
ATTR_OBJ_ID = '_ObjectID'
ATTR_DISPLAY_NAME = 'Display-Name'
ATTR_DEVICE_NAME = 'Device-Name'

# Initialize curl before multiple threads have been started
curl.global_init(curl.GLOBAL_DEFAULT)


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
        return self._attrs.iterkeys()

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
        # Serialize
        attrs = []
        for name in send_keys:
            if name in send_values:
                value = self._attrs[name]
            else:
                value = ''
            attrs.append(XDR_attribute(name, value))
        return attrs

    def xdr(self, output_set=None):
        '''Return an XDR_object.'''
        return XDR_object(self.xdr_attributes(output_set))


class Object(EmptyObject):
    '''A mutable Diamond object.'''

    def __init__(self, server_id, url):
        EmptyObject.__init__(self)
        self._id = url

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
        self._signatures[key] = md5(value).hexdigest()


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
        self._body = StringIO()

    def get(self, url):
        '''Fetch the specified URL and return (header_dict, body).'''
        # Perform the fetch
        self._curl.setopt(curl.URL, url)
        try:
            self._curl.perform()
        except curl.error, e:
            raise ObjectLoadError(e[1])
        # Localize fetched data and release this object's copy
        headers = self._headers
        self._headers = {}
        body = self._body.getvalue()
        self._body = StringIO()
        return (headers, body)

    def _handle_header(self, hdr):
        hdr = hdr.rstrip('\r\n')
        if hdr.startswith('HTTP/'):
            # New HTTP status line, discard existing headers
            self._headers = {}
        elif hdr != '':
            # This is simplistic.
            key, value = hdr.split(': ', 1)
            self._headers[key] = value

    def _handle_body(self, data):
        self._body.write(data)


class ObjectLoader(object):
    '''A context for populating an Object from the dataretriever.  Allows
    network connections to be reused to fetch multiple objects.  Must not
    be used by more than one thread.'''

    def __init__(self, config, blob_cache):
        self._http = _HttpLoader(config)
        self._blob_cache = blob_cache

    def source_available(self, obj):
        '''Examine the Object and return whether we think we will be able
        to load it, without actually doing so.  This can be used during
        reexecution to determine whether we should return
        DiamondRPCFCacheMiss to the client.'''
        scheme, path = split_scheme(str(obj))
        if scheme == self._blob_cache.digest:
            return path in self._blob_cache
        else:
            # Assume we can always load other types of URLs
            return True

    def load(self, obj):
        '''Retrieve the Object and update it with the information we
        receive.'''
        uri = str(obj)
        scheme, path = split_scheme(uri)
        if scheme == self._blob_cache.digest:
            self._load_blobcache(obj, path)
        else:
            self._load_dataretriever(obj, uri)
        # Set display name if not already in initial attributes
        if ATTR_DISPLAY_NAME not in obj:
            obj[ATTR_DISPLAY_NAME] = uri + '\0'

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
        for key, value in headers.iteritems():
            if key.lower().startswith(ATTR_HEADER_PREFIX):
                key = key.replace(ATTR_HEADER_PREFIX, '', 1)
                obj[key] = value + '\0'
        # Fetch additional initial attributes if specified
        if ATTR_HEADER_URL in headers:
            attr_url = urljoin(url, headers[ATTR_HEADER_URL])
            self._load_attributes(obj, attr_url)

    # The return type of json.loads() confuses pylint
    # pylint: disable=E1103
    def _load_attributes(self, obj, url):
        '''Load JSON-encoded attribute data from the specified URL.'''
        _headers, body = self._http.get(url)
        try:
            attrs = json.loads(body)
            if not isinstance(attrs, dict):
                raise ObjectLoadError("Failed to retrieve object attributes")
        except ValueError, e:
            raise ObjectLoadError(str(e))
        for k, v in attrs.iteritems():
            obj[k] = str(v) + '\0'
    # pylint: enable=E1103
