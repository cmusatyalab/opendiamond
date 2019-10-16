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

from builtins import str
from builtins import next
from builtins import object
import binascii
import logging
import uuid
from collections import deque
from hashlib import sha256
import struct
import threading

from opendiamond.client.rpc import ControlConnection, BlastConnection
from opendiamond.protocol import (
    XDR_setup, XDR_filter_config, XDR_blob_data, XDR_start, XDR_reexecute,
    DiamondRPCFCacheMiss)
from opendiamond.rpc import RPCError, ConnectionFailure
from opendiamond.scope import get_cookie_map

_log = logging.getLogger(__name__)

"""
Hierarchy of abstractions:
One DiamondSearch contains one or more _DiamondConnection to multiple
servers, which share the same FilterSpec's.
One _DiamondConnection contains exactly one ControlConnection + one BlastConnection to one destination server.
"""

class Blob(object):
    """An abstract class wrapping some binary data that will be loaded later.
    The data can be retrieved with .data """

    def __init__(self, data=b''):
        self._sha256 = None

        assert isinstance(data, (str, bytes))
        self._data = data if isinstance(data, bytes) else data.encode() # courtesy for str

    @property
    def data(self):
        # bytes
        return self._data

    def __str__(self):
        return '<Blob sha256={}>'.format(self.sha256)

    @property
    def sha256(self):
        # str
        if self._sha256 is None:
            self._sha256 = sha256(self.data).hexdigest()
        return self._sha256


class FilterSpec(object):
    def __init__(
        self, name, code,
        arguments=[],
        blob_argument=Blob(),
        dependencies=[],
        min_score=float('-inf'),
        max_score=float('inf')):
        """Configuration of a filter
        
        Arguments:
            name {str} -- Filter name
            code {Blob} -- Binary blob of code
        
        Keyword Arguments:
            arguments {list of str} -- Filter arguments (default: {[]})
            blob_argument {Blob} -- Binary blob argument (default: {Blob(b'')})
            dependencies {list of str} -- Filter names of dependency (default: {[]})
            min_score {float} -- [description] (default: {float('-inf')})
            max_score {float} -- [description] (default: {float('inf')})
        """     
        self.name = name
        self.code = code
        self.arguments = arguments
        self.blob_argument = blob_argument
        self.dependencies = dependencies
        self.min_score = min_score
        self.max_score = max_score

    def __str__(self):
        return self.name

    def __repr__(self):
        return 'FilterSpec(name={}, code={}, arguments={}, blob_argument={},' \
               'dependencies={}, min_score={}, max_score={})'.format(
            self.name,
            self.code,
            self.arguments,
            self.blob_argument,
            self.dependencies,
            self.min_score,
            self.max_score
        )


class _DiamondConnection(object):
    def __init__(self, address):
        self._finished = False  # No more results
        self._closed = False  # Connection closed
        self.address = address
        self.control = ControlConnection()
        self.blast = BlastConnection()

    def connect(self):
        _log.info("Creating control channel to %s", self.address)
        nonce = self.control.connect(self.address)
        _log.info("Done. Nonce %s", binascii.hexlify(nonce))

        _log.info("Creating blast channel. Nonce %s", binascii.hexlify(nonce))
        self.blast.connect(self.address, nonce)
        _log.info("Done.")

    @staticmethod
    def _blob_uri(blob):
        return u'sha256:' + blob.sha256

    def setup(self, cookies, filters):
        uri_data = {}

        # pre-compute sha_uri -> bytes
        for f in filters:
            uri_data[self._blob_uri(f.code)] = f.code.data
            uri_data[self._blob_uri(f.blob_argument)] = f.blob_argument.data

        # Send setup request
        request = XDR_setup(
            cookies=[c.encode() for c in cookies],
            filters=[XDR_filter_config(
                name=f.name,
                arguments=f.arguments,
                dependencies=f.dependencies,
                min_score=f.min_score,
                max_score=f.max_score,
                code=self._blob_uri(f.code),    # sha256 signature
                blob=self._blob_uri(f.blob_argument)    # sha256 signature
            ) for f in filters],
        )
        reply = self.control.setup(request) 
        # FIXME: xdrlib pack_fstring are pack_fopaque are the same function and appends a b'\0'.
        # This causes error in Python 3.0.
        #https://github.com/python/cpython/blob/master/Lib/xdrlib.py#L103

        # Send uncached blobs if there are any
        blobs = [uri_data[u] for u in reply.uris]   # bytes
        if blobs:
            blob = XDR_blob_data(blobs=blobs)
            self.control.send_blobs(blob)

    def run_search(self, search_id, cookies, filters, attrs=None):
        _log.info("Running search %s", search_id)
        self.connect()
        self.setup(cookies, filters)
        request = XDR_start(search_id=search_id, attrs=attrs)
        self.control.start(request)

    def get_result(self):
        """
        :return: a dictionary of received attribute-value pairs of an object.
        Return will an empty dict when search terminates.
        """
        reply = self.blast.get_object()
        dct = dict((attr.name, attr.value) for attr in reply.attrs)
        if not dct:
            # End of search
            self._finished = True
            dct = None
        else:
            dct = self._sanitize_obj_dict(dct)
        return dct

    def evaluate(self, cookies, filters, blob, attrs=None):
        """
        Also known as re-execution.
        :param cookies:
        :param filters:
        :param blob:
        :param attrs:
        :return: A dictionary of the re-executed object's attribute-value pairs.
        """
        self.connect()
        self.setup(cookies, filters)

        # Send reexecute request
        request = XDR_reexecute(object_id=self._blob_uri(blob), attrs=attrs)
        try:
            reply = self.control.reexecute_filters(request)
        except DiamondRPCFCacheMiss:
            # Send object data and retry
            self.control.send_blobs(XDR_blob_data(blobs=[str(blob)]))
            reply = self.control.reexecute_filters(request)

        # Return object attributes
        dct = dict((attr.name, attr.value) for attr in reply.attrs)
        return self._sanitize_obj_dict(dct)

    reexecute = evaluate    # alias

    def close(self):
        if not self._closed:
            self._closed = True
            self.control.close()
            self.blast.close()

    @staticmethod
    def _sanitize_obj_dict(dct):
        """
        Sanitize object data (a dictionary) directly received from server.
        Empty (non-pushed) attributes will be replaced with None.
        Number types will be unpacked to the corresponding types.
        Strings will remove the trailing '\x00' character.
        :param dct:
        :return:
        """

        def _sanitize_int(s):
            return struct.unpack('i', s)[0]

        def _sanitize_float(s):
            return struct.unpack('f', s)[0]

        def _sanitize_string(s):
            s = s.decode().strip('\0 ')
            return s

        rv = dict()
        for k, v in dct.items():
            if not v:   # empty string means Null (not sent)
                v = None
            else:
                if k.endswith('.int'):
                    v = _sanitize_int(v)
                elif k.endswith('.float'):
                    v = _sanitize_float(v)
                elif k.startswith('_filter') and k.endswith('_score'):
                    # a filter score
                    v = float(_sanitize_string(v))
                else:
                    # assume others are strings
                    v = _sanitize_string(v)

            rv[k] = v

        return rv


class _DiamondBlastSet(object):
    """
    Pool a set of _DiamondConnection's and return results from them as one stream.
    """

    def __init__(self, connections):
        # Connections that have not finished searching
        self._connections = set(connections)
        self._started = False

    def start(self):
        """

        :return: A generator that yields search results from underlying connections.
        """
        assert not self._started
        self._started = True
        return self._try_start()

    def _try_start(self):
        """A generator yielding search results from
        all underlying DiamondConnection's."""
        if self._started:
            pending_objs = deque()
            pending_conns = set()

            def recv_objs(conn):
                try:
                    handler = _DiamondBlastSet._handle_objects(conn)
                    for obj in handler:
                        pending_objs.append(obj)
                finally:
                    pending_conns.remove(conn)

            for conn in self._connections:
                pending_conns.add(conn)
                worker = threading.Thread(target=recv_objs, args=(conn,))
                worker.daemon = True
                worker.start()

            while pending_objs or pending_conns:
                if pending_objs:
                    yield pending_objs.popleft()


    @staticmethod
    def _handle_objects(conn):
        """A generator yielding search results from a DiamondConnection."""
        while True:
            try:
                dct = conn.get_result()
            except ConnectionFailure:
                break
            except RPCError:
                _log.exception('Cannot receive blast object')
                conn.close()
                break

            if dct is None:
                break

            yield dct


class DiamondSearch(object):
    def __init__(self, cookies, filters, push_attrs=None):
        """

        :param cookies: A list of ScopeCookie
        :param filters: A list of FilterSpec
        """
        self._push_attrs = push_attrs
        self.results = None  # will bind to a generator when the search starts

        self._closed = False

        # host -> [cookie]
        self._cookie_map = get_cookie_map(cookies)
        self._filters = filters

        # host -> _DiamondConnection
        self._connections = dict((h, _DiamondConnection(h))
                                 for h in self._cookie_map)
        self._blast = _DiamondBlastSet(list(self._connections.values()))

    def start(self):
        """
        Start the search and bind a generator to self.results which yields
        return objects (as dictionaries) from all underlying connections.
        :return: A random-generated Search ID.
        """
        search_id = str(uuid.uuid4()) # must be 36 chars long to conform to protocol

        for h, c in list(self._connections.items()):
            try:
                c.run_search(search_id.encode(), self._cookie_map[h], self._filters,
                             self._push_attrs)
            except:
                _log.error("Can't start search on %s. "
                           "May be expired cookie, corrupted filters, "
                           "network failure, service not running, "
                           "no space on disk, etc.?", h)
                raise

        # Start blast channels
        assert self.results is None
        self.results = self._blast.start()
        return search_id

    def evaluate(self, blob, callback=None):
        """
        Also known as re-execution. The object to re-execute may be an external object.
        :param blob: A Blob object wrapping the object to re-execute.
        :param callback:
        :return:
        """
        # Try to pick the same server for the same blob
        server_index = abs(hash(blob.sha256)) % len(self._connections)
        hostname = sorted(self._connections)[server_index]
        conn = self._connections[hostname]

        # Reexecute
        obj = conn.evaluate(self._cookie_map[hostname], self._filters, blob)
        if callback is not None:
            callback(obj)

    def get_stats(self):
        """
        Send statistics requests to all connections and aggregate the returned stats.
        :return:
        """

        def combine_into(dest, src):
            for stat in src:
                dest.setdefault(stat.name, 0)
                dest[stat.name] += stat.value

        try:
            results = [c.control.request_stats() for c in
                       list(self._connections.values())]
            stats = {}
            filter_stats = {}
            for result in results:
                combine_into(stats, result.stats)
                for f in result.filter_stats:
                    combine_into(filter_stats.setdefault(f.name, {}),
                                 f.stats)
            stats['filters'] = filter_stats
            return stats
        except RPCError:
            _log.exception('Statistics request failed')
            self.close()

    def close(self):
        if not self._closed:
            for conn in list(self._connections.values()):
                conn.close()
            self._closed = True
