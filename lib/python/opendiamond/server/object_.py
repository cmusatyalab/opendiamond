#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2002-2007 Intel Corporation
#  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
#  Copyright (c) 2006-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from hashlib import md5
from urllib2 import urlopen

from opendiamond.server.protocol import XDR_attribute, XDR_object

ATTR_HEADER_PREFIX = 'x-attr-'
# Object attributes handled directly by the server
ATTR_DATA = ''
ATTR_OBJ_ID = '_ObjectID'
ATTR_DISPLAY_NAME = 'Display-Name'
ATTR_DEVICE_NAME = 'Device-Name'

class EmptyObject(object):
    id = 'Empty'

    def __init__(self):
        self._attrs = dict()
        self._signatures = dict()
        self._omit_attrs = set()

    def __repr__(self):
        return '<Object(%s)>' % repr(self.id)

    def load(self):
        raise TypeError()

    def __iter__(self):
        return self._attrs.iterkeys()

    def __contains__(self, key):
        return key in self._attrs

    def __getitem__(self, key):
        return self._attrs[key]

    def __setitem__(self, key, value):
        raise TypeError()

    def get_signature(self, key):
        return self._signatures[key]

    def omit(self, key):
        if key in self:
            self._omit_attrs.add(key)
        else:
            raise KeyError()

    def xdr_attributes(self, output_set=None, with_data=True):
        '''Returns a list of XDR_attribute.'''
        # Don't encode any evidence of omit attributes.
        send_keys = set(self._attrs.keys()) - self._omit_attrs
        # XDR_object never encodes the data attribute, because it has a
        # separate field for the object data, but XDR_attribute_list can
        # encode it with a suitable output_set.
        if not with_data:
            send_keys.discard(ATTR_DATA)
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

    def xdr(self, search_id, output_set=None):
        return XDR_object(search_id, self._attrs.get(ATTR_DATA, ''),
                            self.xdr_attributes(output_set, False))


class Object(EmptyObject):
    def __init__(self, server_id, url):
        EmptyObject.__init__(self)
        self.id = url

        # Set default attributes
        self[ATTR_DEVICE_NAME] = server_id + '\0'
        self[ATTR_OBJ_ID] = url + '\0'

    def load(self):
        # Load the object data.  urllib2 does not reuse HTTP connections, so
        # this will always create a new connection.  If this becomes a
        # problem, this code will need to be converted to use pycurl.
        # The dataretriever may not support persistent connections either,
        # depending on the HTTP server package it's using, so the lack of
        # support on this end may not matter.
        fh = urlopen(self.id)
        self[ATTR_DATA] = fh.read()
        # Process initial attributes
        info = fh.info()
        for header in info.keys():
            if header.lower().startswith(ATTR_HEADER_PREFIX):
                attr = header.replace(ATTR_HEADER_PREFIX, '', 1)
                self[attr] = info[header] + '\0'
        # Set display name if not already in initial attributes
        if ATTR_DISPLAY_NAME not in self:
            self[ATTR_DISPLAY_NAME] = self.id + '\0'

    def __setitem__(self, key, value):
        self._attrs[key] = value
        self._signatures[key] = md5(value).hexdigest()
