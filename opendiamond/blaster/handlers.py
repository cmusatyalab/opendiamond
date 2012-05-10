#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2012 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''JSON Blaster request handlers.'''

import simplejson as json
import os
import cPickle as pickle
from urlparse import urlparse
from tornadio2 import SocketConnection, event
from tornado.curl_httpclient import CurlAsyncHTTPClient as AsyncHTTPClient
from tornado import gen
from tornado.options import define, options
from tornado.web import asynchronous, RequestHandler, HTTPError
import validictory

import opendiamond
from opendiamond.blaster.search import Blob, EmptyBlob
from opendiamond.helpers import sha256
from opendiamond.scope import ScopeCookie, ScopeError

CACHE_URN_SCHEME = 'blob'

# HTTP method handlers have specific argument lists rather than
# (self, *args, **kwargs) as in the superclass.
# pylint: disable=W0221

define('enable_testui', default=True,
        help='Enable the example user interface')
define('http_proxy', type=str, default=None,
        metavar='HOST:PORT', help='Use a proxy for HTTP client requests')


def _load_schema(name):
    with open(os.path.join(os.path.dirname(__file__), name)) as fh:
        return json.load(fh)
SEARCH_SCHEMA = _load_schema('schema-search.json')


class _BlasterRequestHandler(RequestHandler):
    @property
    def blob_cache(self):
        return self.application.blob_cache

    @property
    def search_spec_cache(self):
        return self.application.search_spec_cache

    def write_error(self, code, **kwargs):
        exc_type, exc_value, _exc_tb = kwargs.get('exc_info', [None] * 3)
        if exc_type is not None and issubclass(exc_type, HTTPError):
            self.set_header('Content-Type', 'text/plain')
            if exc_value.log_message:
                self.write(exc_value.log_message + '\n')
        else:
            RequestHandler.write_error(self, code, **kwargs)


class _BlasterBlob(Blob):
    '''Instances with the same URI compare equal and hash to the same value.'''

    def __init__(self, uri, expected_sha256=None):
        Blob.__init__(self)
        self.uri = uri
        self._expected_sha256 = expected_sha256
        self._data = None

    def __str__(self):
        if self._data is None:
            raise RuntimeError('Attempting to read an unfetched blob')
        return self._data

    def __repr__(self):
        return '<_BlasterBlob %s>' % (self.uri)

    def __hash__(self):
        return hash(self.uri)

    def __eq__(self, other):
        return isinstance(other, type(self)) and self.uri == other.uri

    # pylint doesn't understand named tuples
    # pylint: disable=E1101
    @gen.engine
    def fetch(self, blob_cache, callback=None):
        if self._data is None:
            # Fetch data
            parts = urlparse(self.uri)
            if parts.scheme == 'blob':
                try:
                    data = blob_cache[parts.path]
                except KeyError:
                    raise HTTPError(400, 'Blob missing from blob cache')
            elif parts.scheme == 'http' or parts.scheme == 'https':
                client = AsyncHTTPClient()
                if options.http_proxy is not None:
                    proxy_host, proxy_port = options.http_proxy.split(':', 1)
                    proxy_port = int(proxy_port)
                else:
                    proxy_host = proxy_port = None
                response = yield gen.Task(client.fetch, self.uri,
                        user_agent='JSONBlaster/%s' % opendiamond.__version__,
                        proxy_host=proxy_host, proxy_port=proxy_port,
                        validate_cert=False)
                if response.error:
                    raise HTTPError(400, 'Error fetching <%s>: %s' % (
                            self.uri, str(response.error)))
                data = response.body
            else:
                raise HTTPError(400, 'Unacceptable blob URI scheme')

            # Check hash if requested
            if self._expected_sha256 is not None:
                if sha256(data).hexdigest() != self._expected_sha256:
                    raise HTTPError(400, 'SHA-256 mismatch on %s' % self.uri)

            # Commit
            self._data = data

        if callback is not None:
            callback()
    # pylint: enable=E1101


class _SearchSpec(object):
    def __init__(self, data):
        # Load JSON
        try:
            config = json.loads(data)
            # required_by_default=False and blank_by_default=True for
            # JSON Schema draft 3 semantics
            validictory.validate(config, SEARCH_SCHEMA,
                    required_by_default=False, blank_by_default=True)
        except ValueError, e:
            raise HTTPError(400, str(e))

        # Build cookies
        # Assume each "cookie" may actually be a megacookie
        try:
            self.cookies = [ScopeCookie.parse(c) for mc in config['cookies']
                    for c in ScopeCookie.split(mc)]
        except ScopeError, e:
            raise HTTPError(400, 'Invalid scope cookie: %s' % e)
        if not self.cookies:
            # No cookies could be parsed out of the client's cookie list
            raise HTTPError(400, 'No scope cookies found')

        # Build blob list
        blobs = {}  # blob -> itself  (for deduplication)
        def make_blob(obj):
            if obj is not None:
                blob = _BlasterBlob(obj['uri'], obj.get('sha256'))
                # Intern the blob
                return blobs.setdefault(blob, blob)
            else:
                return EmptyBlob()
        blob_list = [make_blob(f['code']) for f in config['filters']] + \
                [make_blob(f.get('blob')) for f in config['filters']]
        self.blobs = blobs.values()

    @gen.engine
    def fetch_blobs(self, blob_cache, callback=None):
        yield [gen.Task(blob.fetch, blob_cache) for blob in self.blobs]
        if callback is not None:
            callback()


class SearchHandler(_BlasterRequestHandler):
    def get(self):
        if options.enable_testui:
            self.render('testui/search.html')
        else:
            raise HTTPError(405, 'Method not allowed')

    @asynchronous
    @gen.engine
    def post(self):
        # Build search spec
        if self.request.headers['Content-Type'] != 'application/json':
            raise HTTPError(415, 'Content type must be application/json')
        spec = _SearchSpec(self.request.body)
        yield gen.Task(spec.fetch_blobs, self.blob_cache)

        # Store it
        pickled = pickle.dumps(spec, pickle.HIGHEST_PROTOCOL)
        search_key = self.search_spec_cache.add(pickled)

        # Return result
        self.set_status(204)
        self.set_header('X-Search-Key', search_key)
        self.finish()


class PostBlobHandler(_BlasterRequestHandler):
    def post(self):
        sig = self.blob_cache.add(self.request.body)
        self.set_header('Location', '%s:%s' % (CACHE_URN_SCHEME, sig))
        self.set_status(204)


class ResultsHandler(_BlasterRequestHandler):
    def get(self):
        if options.enable_testui:
            self.render('testui/results.html')
        else:
            raise HTTPError(403, 'Forbidden')


class _TestSearch(object):
    def __init__(self, search_key, callback):
        from tornado.ioloop import PeriodicCallback
        self._search_key = search_key
        self._callback = callback
        self._count = 0
        self._timer = PeriodicCallback(self._result, 1000)
        self._timer.start()

    def _result(self):
        self._callback({
            'count': self._count,
            'search_key': self._search_key
        })
        self._count += 1

    def pause(self):
        self._timer.stop()

    def resume(self):
        self._timer.start()

    def close(self):
        self._timer.stop()


class SearchConnection(SocketConnection):
    def __init__(self, *args, **kwargs):
        SocketConnection.__init__(self, *args, **kwargs)
        self._search = None

    def on_message(self, _message):
        # Must be overridden; superclass is abstract
        raise HTTPError(400, 'Cannot send messages here')

    @event
    def start(self, search_key):
        '''Ideally this would be handled in on_open(), but query parameters
        are per-socket rather than per-connection, so we can't reliably pass
        the search key when opening the connection (socket.io-client #331).
        So we have a separate start event instead.'''

        # Sanity checks
        if self._search is not None:
            raise HTTPError(400, 'Search already started')

        # Load the search spec
        search_spec_cache = self.session.handler.application.search_spec_cache
        try:
            pickled = search_spec_cache[search_key]
        except KeyError:
            raise HTTPError(400, 'Invalid search key')
        try:
            search_spec = pickle.loads(pickled)
        except Exception:
            raise HTTPError(400, 'Corrupt search key')

        # Start the search
        print 'start', search_key
        self._search = _TestSearch(search_key, self._result)

    def _result(self, data):
        self.emit('result', **data)

    @event
    def pause(self):
        if self._search is None:
            raise HTTPError(400, 'Search not yet started')
        self._search.pause()

    @event
    def resume(self):
        if self._search is None:
            raise HTTPError(400, 'Search not yet started')
        self._search.resume()

    def on_close(self):
        print 'close'
        if self._search is not None:
            self._search.close()
