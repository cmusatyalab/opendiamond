#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Search state; control and blast channel handling.'''

from functools import wraps
import logging

from opendiamond.scope import ScopeCookie, ScopeError, ScopeCookieExpired
from opendiamond.server.blobcache import BlobCache
from opendiamond.server.filter import FilterStack
from opendiamond.server.object_ import EmptyObject, Object
from opendiamond.server.protocol import *
from opendiamond.server.rpc import RPCProcedureUnavailable
from opendiamond.server.scopelist import ScopeListLoader
from opendiamond.server.sessionvars import SessionVariables
from opendiamond.server.statistics import SearchStatistics

_log = logging.getLogger(__name__)

class SearchState(object):
    '''Search state that is also needed by filter code.'''
    def __init__(self, config):
        self.config = config
        self.blob_cache = BlobCache(config.cachedir)
        self.session_vars = SessionVariables()
        self.stats = SearchStatistics()
        self.scope = None
        self.blast = None


class Search(RPCHandlers):
    '''State for a single search, plus handlers for control channel RPCs
    to modify it.'''

    log_rpcs = True

    def __init__(self, config, blast_conn):
        RPCHandlers.__init__(self)
        self._server_id = config.serverids[0]  # Canonical server ID
        self._blast_conn = blast_conn
        self._state = SearchState(config)
        self._filters = FilterStack()
        self._cookies = []
        self._push_attrs = None
        self._running = False

    def __del__(self):
        # Log search statistics
        if self._running:
            self._state.stats.log()
            for filter in self._filters:
                filter.stats.log()

    def running(should_be_running):
        '''Decorator that specifies that the handler can only be called
        before, or after, the search has started running.  This is only
        called when initializing the class, which is why it's not a
        static method.'''
        def decorator(f):
            @wraps(f)
            def wrapper(self, *args, **kwargs):
                if self._running != should_be_running:
                    raise RPCProcedureUnavailable()
                return f(self, *args, **kwargs)
            return wrapper
        return decorator

    def _check_runnable(self):
        '''Validate state preparatory to starting a search or reexecution.'''
        if self._state.scope is None:
            raise DiamondRPCFailure('No search scope configured')
        if len(self._filters) == 0:
            raise DiamondRPCFailure('No filters configured')

    @RPCHandlers.handler(24, XDR_scope)
    @running(False)
    def set_scope(self, params):
        '''Add a scope cookie to the cookie list.'''
        try:
            cookie = ScopeCookie.parse(params.cookie)
            _log.info('Received scope cookie %s', repr(cookie))
            cookie.verify(self._state.config.serverids,
                            self._state.config.certdata)
            self._cookies.append(cookie)
            self._state.scope = ScopeListLoader(self._server_id, self._cookies)
        except ScopeCookieExpired, e:
            _log.warning('%s', e)
            raise DiamondRPCCookieExpired()
        except ScopeError, e:
            _log.warning('Cookie invalid: %s', e)
            raise DiamondRPCFailure()

    @RPCHandlers.handler(20, XDR_attr_name_list)
    @running(False)
    def set_push_attrs(self, params):
        '''Configure the list of object attributes for which the client
        would like values as well as names.'''
        self._push_attrs = set(params.attrs)

    @RPCHandlers.handler(6, XDR_spec_file)
    @running(False)
    def set_spec(self, params):
        '''Define the filter stack.'''
        self._filters = FilterStack.from_fspec(params.data)

    @RPCHandlers.handler(16, XDR_sig_val)
    @running(False)
    def set_filter(self, params):
        '''Inquire whether the specified filter code is already cached.'''
        if params.sig not in self._state.blob_cache:
            raise DiamondRPCFCacheMiss()

    @RPCHandlers.handler(17, XDR_filter)
    @running(False)
    def send_filter(self, params):
        '''Provide new filter code for the cache.'''
        self._state.blob_cache.add(params.data)

    @RPCHandlers.handler(11, XDR_blob)
    @running(False)
    def set_blob(self, params):
        '''Bind the specified filter blob argument to the named filter.'''
        self._state.blob_cache.add(params.data)
        try:
            self._filters[params.filter_name].blob = params.data
        except KeyError:
            raise DiamondRPCFailure()

    @RPCHandlers.handler(22, XDR_blob_sig)
    @running(False)
    def set_blob_by_signature(self, params):
        '''Try to bind a cached filter blob argument to the named filter.'''
        try:
            data = self._state.blob_cache[params.sig.sig]
        except KeyError:
            raise DiamondRPCFCacheMiss()
        try:
            self._filters[params.filter_name].blob = data
        except KeyError:
            raise DiamondRPCFailure()

    @RPCHandlers.handler(1, XDR_start)
    @running(False)
    def start(self, params):
        '''Start the search.'''
        try:
            self._check_runnable()
        except DiamondRPCFailure, e:
            _log.warning('Cannot start search: %s', str(e))
            raise
        self._state.blast = BlastChannel(self._blast_conn, params.search_id,
                                self._push_attrs)
        self._running = True
        _log.info('Starting search %d', params.search_id)
        self._filters.start_threads(self._state, self._state.config.threads)

    @RPCHandlers.handler(21, XDR_reexecute, XDR_attribute_list)
    def reexecute_filters(self, params):
        '''Reexecute the search on the specified object.'''
        try:
            self._check_runnable()
        except DiamondRPCFailure, e:
            _log.warning('Cannot reexecute filters: %s', str(e))
            raise
        _log.info('Reexecuting on object %s', params.object_id)
        runner = self._filters.bind(self._state)
        obj = Object(self._server_id, params.object_id)
        runner.evaluate(obj)
        if len(params.attrs):
            output_attrs = set(params.attrs)
        else:
            # If no output attributes were specified, encode everything
            output_attrs = None
        return XDR_attribute_list(obj.xdr_attributes(output_attrs))

    @RPCHandlers.handler(15, reply_class=XDR_search_stats)
    @running(True)
    def request_stats(self):
        '''Return current search statistics.'''
        filter_stats = [f.stats for f in self._filters]
        return self._state.stats.xdr(self._state.scope.get_count(),
                            filter_stats)

    @RPCHandlers.handler(18, reply_class=XDR_session_vars)
    @running(True)
    def session_variables_get(self):
        '''Return partial values for all session variables.'''
        vars = [XDR_session_var(name=name, value=value)
                for name, value in
                self._state.session_vars.client_get().iteritems()]
        return XDR_session_vars(vars=vars)

    @RPCHandlers.handler(19, XDR_session_vars)
    @running(True)
    def session_variables_set(self, params):
        '''Integrate new merged values for all session variables.'''
        values = dict()
        for var in params.vars:
            values[var.name] = var.value
        self._state.session_vars.client_set(values)


class _BlastChannelSender(RPCHandlers):
    '''Single-use RPC handler for sending an XDR_object on the blast
    channel.'''

    def __init__(self, obj):
        self._obj = obj
        self._sent = False

    @RPCHandlers.handler(1, reply_class=XDR_object)
    def get_object(self):
        '''Return an accepted object.'''
        assert not self._sent
        self._sent = True
        return self._obj

    def send(self, conn):
        '''Send the object on the blast connection.'''
        while not self._sent:
            conn.dispatch(self)


class BlastChannel(object):
    '''A wrapper for a blast channel connection.'''

    def __init__(self, conn, search_id, push_attrs):
        self._conn = conn
        self._search_id = search_id
        self._push_attrs = push_attrs

    def send(self, obj):
        '''Send the specified Object on the blast channel.'''
        xdr = obj.xdr(self._search_id, self._push_attrs)
        _BlastChannelSender(xdr).send(self._conn)

    def close(self):
        '''Tell the client that no more objects will be returned.'''
        xdr = EmptyObject().xdr(self._search_id)
        _BlastChannelSender(xdr).send(self._conn)
