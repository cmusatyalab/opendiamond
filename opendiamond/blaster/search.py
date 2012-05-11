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

import logging
import random
from tornado import gen, stack_context

from opendiamond.blaster.rpc import ControlConnection, BlastConnection
from opendiamond.helpers import md5
from opendiamond.protocol import (XDR_setup, XDR_filter_config,
        XDR_blob_data, XDR_start)
from opendiamond.rpc import RPCError
from opendiamond.scope import get_cookie_map

_log = logging.getLogger(__name__)


class Blob(object):
    '''An abstract class wrapping some binary data that will be loaded later.
    The data can be retrieved with str().'''

    def __init__(self):
        self._md5 = None

    def __str__(self):
        raise NotImplementedError()

    def __repr__(self):
        return '<Blob>'

    @property
    def md5(self):
        if self._md5 is None:
            self._md5 = md5(str(self)).hexdigest()
        return self._md5


class EmptyBlob(Blob):
    '''An empty blob argument.'''

    def __str__(self):
        return ''

    def __hash__(self):
        return 12345

    def __eq__(self, other):
        return type(self) is type(other)


class FilterSpec(object):
    def __init__(self, name, code, arguments, blob_argument, dependencies,
            min_score, max_score):
        '''code and blob are Blobs.'''
        self.name = name
        self.code = code
        self.arguments = arguments
        self.blob_argument = blob_argument
        self.dependencies = dependencies
        self.min_score = min_score
        self.max_score = max_score


class _DiamondConnection(object):
    def __init__(self, address, close_callback):
        self._close_callback = stack_context.wrap(close_callback)
        self._finished = False  # No more results
        self._closed = False    # Connection closed
        self.address = address
        self.control = ControlConnection(self.close)
        self.blast = BlastConnection(self.close)

    @gen.engine
    def connect(self, callback=None):
        # On connection failure, the Tasks will not return and self.close()
        # will be called
        nonce = yield gen.Task(self.control.connect, self.address)
        yield gen.Task(self.blast.connect, self.address, nonce)
        if callback is not None:
            callback()

    @gen.engine
    def setup(self, cookies, filters, callback=None):
        def uri(md5sum):
            return 'md5:' + md5sum

        uri_data = {}
        for f in filters:
            uri_data[uri(f.code.md5)] = str(f.code)
            uri_data[uri(f.blob_argument.md5)] = str(f.blob_argument)

        # Send setup request
        request = XDR_setup(
            cookies=[c.encode() for c in cookies],
            filters=[XDR_filter_config(
                        name=f.name,
                        arguments=f.arguments,
                        dependencies=f.dependencies,
                        min_score=f.min_score,
                        max_score=f.max_score,
                        code=uri(f.code.md5),
                        blob=uri(f.blob_argument.md5)
                    ) for f in filters],
        )
        reply = yield gen.Task(self.control.setup, request)

        # Send uncached blobs if there are any
        blobs = [uri_data[u] for u in reply.uris]
        if blobs:
            request = XDR_blob_data(blobs=blobs)
            yield gen.Task(self.control.send_blobs, request)

        if callback is not None:
            callback()

    @gen.engine
    def run_search(self, search_id, cookies, filters, attrs=None,
            callback=None):
        yield gen.Task(self.connect)
        yield gen.Task(self.setup, cookies, filters)
        request = XDR_start(search_id=search_id, attrs=attrs)
        yield gen.Task(self.control.start, request)
        if callback is not None:
            callback()

    @gen.engine
    def get_result(self, callback=None):
        if callback is not None and self._finished:
            callback(None)
            return
        reply = yield gen.Task(self.blast.get_object)
        object = dict((attr.name, attr.value) for attr in reply.attrs)
        if not object:
            # End of search
            self._finished = True
            object = None
        if callback is not None:
            callback(object)

    def close(self):
        if not self._closed:
            self._closed = True
            self.control.close()
            self.blast.close()
            if self._close_callback is not None:
                self._close_callback()


class _DiamondBlastSet(object):
    def __init__(self, connections, object_callback=None,
            finished_callback=None):
        self._object_callback = stack_context.wrap(object_callback)
        self._finished_callback = stack_context.wrap(finished_callback)
        # Connections that have not finished searching
        self._connections = set(connections)
        # Connections without an outstanding blast request
        self._paused = set(connections)
        # False if we are paused
        self._running = False

    def pause(self):
        self._running = False

    def resume(self):
        if not self._running:
            self._running = True
            restart = self._paused
            self._paused = set()
            for conn in restart:
                # Restart handler coroutine
                self._handle_objects(conn)

    @gen.engine
    def _handle_objects(self, conn):
        '''Coroutine that requests blast channel objects on the specified
        connection until the blast set is paused or the connection finishes
        searching.'''
        while self._running:
            try:
                obj = yield gen.Task(conn.get_result)
            except RPCError:
                _log.exception('Cannot receive blast object')
                conn.close()
                return

            if obj is None:
                # Connection has finished searching
                self._connections.discard(conn)
                self._paused.discard(conn)
                if (self._finished_callback is not None
                        and not self._connections):
                    # All connections have finished searching
                    self._finished_callback()
                return

            if self._object_callback is not None:
                self._object_callback(obj)
        self._paused.add(conn)


class DiamondSearch(object):
    def __init__(self, cookies, filters, object_callback=None,
            finished_callback=None, close_callback=None):
        '''cookies is a list of ScopeCookie.  filters is a list of
        FilterSpec.'''

        self._closed = False

        self._close_callback = stack_context.wrap(close_callback)

        # host -> [cookie]
        self._cookies = get_cookie_map(cookies)
        self._filters = filters

        # hostname -> connection
        self._connections = dict((h, _DiamondConnection(h, self.close))
                for h in self._cookies)
        self._blast = _DiamondBlastSet(self._connections.values(),
                object_callback, finished_callback)

    @gen.engine
    def start(self, callback=None):
        search_id = int(random.getrandbits(31))
        # On connection error, our close callback will run and this will
        # never return
        yield [gen.Task(c.run_search, search_id, self._cookies[h],
                self._filters) for h, c in self._connections.iteritems()]
        # Start blast channels
        self.resume()
        if callback is not None:
            callback(search_id)

    def pause(self):
        self._blast.pause()

    def resume(self):
        self._blast.resume()

    def close(self):
        if not self._closed:
            self._closed = True
            for conn in self._connections.values():
                conn.close()
            if self._close_callback is not None:
                self._close_callback()
