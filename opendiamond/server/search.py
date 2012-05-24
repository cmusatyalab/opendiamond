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

'''Search state; control and blast channel handling.'''

from functools import wraps
import logging

from opendiamond import protocol
from opendiamond.blobcache import BlobCache
from opendiamond.protocol import (DiamondRPCFailure, DiamondRPCFCacheMiss,
        DiamondRPCCookieExpired, DiamondRPCSchemeNotSupported)
from opendiamond.rpc import RPCHandlers, RPCError, RPCProcedureUnavailable
from opendiamond.scope import ScopeCookie, ScopeError, ScopeCookieExpired
from opendiamond.server.filter import (FilterStack, Filter,
        FilterDependencyError, FilterUnsupportedSource)
from opendiamond.server.object_ import EmptyObject, Object, ObjectLoader
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
        self._running = False

    def shutdown(self):
        '''Clean up the search before the process exits.'''
        # Log search statistics
        if self._running:
            self._state.stats.log()
            for filter in self._filters:
                filter.stats.log()

    # This is not a static method: it's only called when initializing the
    # class, and the staticmethod() decorator does not create a callable.
    # Also avoid complaints about accesses to self._running
    # pylint: disable=E0213,W0212
    def running(should_be_running):
        '''Decorator that specifies that the handler can only be called
        before, or after, the search has started running.'''
        def decorator(func):
            @wraps(func)
            def wrapper(self, *args, **kwargs):
                if self._running != should_be_running:
                    raise RPCProcedureUnavailable()
                return func(self, *args, **kwargs)
            return wrapper
        return decorator
    # pylint: enable=E0213,W0212

    def _check_runnable(self):
        '''Validate state preparatory to starting a search or reexecution.'''
        if self._state.scope is None:
            raise DiamondRPCFailure('No search scope configured')
        if len(self._filters) == 0:
            raise DiamondRPCFailure('No filters configured')
        # Ensure we have all filter code and blob arguments
        try:
            for filter in self._filters:
                filter.resolve(self._state)
        except FilterDependencyError, e:
            raise DiamondRPCFCacheMiss(str(e))

    @RPCHandlers.handler(25, protocol.XDR_setup, protocol.XDR_blob_list)
    @running(False)
    def setup(self, params):
        '''Configure the search and return a list of SHA256 signatures not
        present in the blob cache.'''
        def log_header(desc):
            _log.info('  %s:', desc)
        def log_item(key, fmt, *args):
            _log.info('    %-14s ' + fmt, key + ':', *args)

        # Create filter stack
        filters = []
        missing = set()
        _log.info('Filters:')
        for f in params.filters:
            unsupported = False
            try:
                if not Filter.source_available(self._state, f.code):
                    missing.add(f.code)
                    code_state = 'not cached'
                else:
                    code_state = 'cached'
            except FilterUnsupportedSource:
                unsupported = True
                code_state = 'unsupported'
            try:
                if not Filter.source_available(self._state, f.blob):
                    missing.add(f.blob)
                    blob_state = 'not cached'
                else:
                    blob_state = 'cached'
            except FilterUnsupportedSource:
                unsupported = True
                blob_state = 'unsupported'
            log_header(f.name)
            log_item('Code', '%s, %s', f.code, code_state)
            log_item('Blob', '%s, %s', f.blob, blob_state)
            log_item('Arguments', '%s', ', '.join(f.arguments) or '<none>')
            log_item('Dependencies', '%s',
                            ', '.join(f.dependencies) or '<none>')
            log_item('Minimum score', '%f', f.min_score)
            log_item('Maximum score', '%f', f.max_score)
            if unsupported:
                raise DiamondRPCSchemeNotSupported()
            filters.append(Filter(f.name, f.code, f.blob, f.min_score,
                            f.max_score, f.arguments, f.dependencies))
        filters = FilterStack(filters)

        # Parse scope cookies
        try:
            cookies = [ScopeCookie.parse(c) for c in params.cookies]
            _log.info('Scope cookies:')
            for cookie in cookies:
                log_header(cookie.serial)
                log_item('Servers', '%s', ', '.join(cookie.servers))
                log_item('Expires', '%s', cookie.expires)
                cookie.verify(self._state.config.serverids,
                                self._state.config.certdata)
            scope = ScopeListLoader(self._state.config, self._server_id,
                                cookies)
        except ScopeCookieExpired, e:
            _log.warning('%s', e)
            raise DiamondRPCCookieExpired()
        except ScopeError, e:
            _log.warning('Cookie invalid: %s', e)
            raise DiamondRPCFailure()

        # Commit
        self._filters = filters
        self._state.scope = scope
        return protocol.XDR_blob_list(missing)

    @RPCHandlers.handler(26, protocol.XDR_blob_data)
    @running(False)
    def send_blobs(self, params):
        '''Add blobs to the blob cache.'''
        _log.info('Received %d blobs, %d bytes', len(params.blobs),
                    sum([len(b) for b in params.blobs]))
        for blob in params.blobs:
            self._state.blob_cache.add(blob)

    @RPCHandlers.handler(28, protocol.XDR_start)
    @running(False)
    def start(self, params):
        '''Start the search.'''
        try:
            self._check_runnable()
        except RPCError, e:
            _log.warning('Cannot start search: %s', str(e))
            raise
        if params.attrs is not None:
            push_attrs = set(params.attrs)
        else:
            # Encode everything
            push_attrs = None
        self._state.blast = BlastChannel(self._blast_conn, push_attrs)
        self._running = True
        _log.info('Starting search %s', params.search_id)
        self._filters.start_threads(self._state, self._state.config.threads)

    @RPCHandlers.handler(21, protocol.XDR_reexecute,
                             protocol.XDR_attribute_list)
    def reexecute_filters(self, params):
        '''Reexecute the search on the specified object.'''
        try:
            self._check_runnable()
        except RPCError, e:
            _log.warning('Cannot reexecute filters: %s', str(e))
            raise
        _log.info('Reexecuting on object %s', params.object_id)
        runner = self._filters.bind(self._state)
        obj = Object(self._server_id, params.object_id)
        loader = ObjectLoader(self._state.config, self._state.blob_cache)
        if not loader.source_available(obj):
            raise DiamondRPCFCacheMiss()
        drop = not runner.evaluate(obj)
        if len(params.attrs):
            output_attrs = set(params.attrs)
        else:
            # If no output attributes were specified, encode everything
            output_attrs = None
        return protocol.XDR_attribute_list(obj.xdr_attributes(output_attrs,
                                for_drop=drop))

    @RPCHandlers.handler(15, reply_class=protocol.XDR_search_stats)
    @running(True)
    def request_stats(self):
        '''Return current search statistics.'''
        filter_stats = [f.stats for f in self._filters]
        return self._state.stats.xdr(self._state.scope.get_count(),
                            filter_stats)

    @RPCHandlers.handler(18, reply_class=protocol.XDR_session_vars)
    @running(True)
    def session_variables_get(self):
        '''Return partial values for all session variables.'''
        vars = [protocol.XDR_session_var(name=name, value=value)
                for name, value in
                self._state.session_vars.client_get().iteritems()]
        return protocol.XDR_session_vars(vars=vars)

    @RPCHandlers.handler(19, protocol.XDR_session_vars)
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
        RPCHandlers.__init__(self)
        self._obj = obj
        self._sent = False

    @RPCHandlers.handler(2, reply_class=protocol.XDR_object)
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

    def __init__(self, conn, push_attrs):
        self._conn = conn
        self._push_attrs = push_attrs

    def send(self, obj):
        '''Send the specified Object on the blast channel.'''
        xdr = obj.xdr(self._push_attrs)
        _BlastChannelSender(xdr).send(self._conn)

    def close(self):
        '''Tell the client that no more objects will be returned.'''
        xdr = EmptyObject().xdr()
        _BlastChannelSender(xdr).send(self._conn)
