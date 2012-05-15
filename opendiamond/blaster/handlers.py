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

from datetime import timedelta
import logging
import simplejson as json
import os
from sockjs.tornado import SockJSConnection
from urlparse import urlparse
from tornado.curl_httpclient import CurlAsyncHTTPClient as AsyncHTTPClient
from tornado import gen
from tornado.ioloop import IOLoop
from tornado.options import define, options
from tornado.web import asynchronous, RequestHandler, HTTPError
import validictory

import opendiamond
from opendiamond.attributes import (StringAttributeCodec,
        IntegerAttributeCodec, DoubleAttributeCodec, PatchesAttributeCodec)
from opendiamond.blaster.cache import SearchCacheLoadError
from opendiamond.blaster.search import (Blob, EmptyBlob, DiamondSearch,
        FilterSpec)
from opendiamond.helpers import sha256
from opendiamond.scope import ScopeCookie, ScopeError

CACHE_URN_SCHEME = 'blob'
STATS_INTERVAL = timedelta(milliseconds=1000)

# HTTP method handlers have specific argument lists rather than
# (self, *args, **kwargs) as in the superclass.
# pylint: disable=W0221

define('enable_testui', default=True,
        help='Enable the example user interface')
define('http_proxy', type=str, default=None,
        metavar='HOST:PORT', help='Use a proxy for HTTP client requests')


_log = logging.getLogger(__name__)


def _load_schema(name):
    with open(os.path.join(os.path.dirname(__file__), name)) as fh:
        return json.load(fh)
EVENT_SCHEMA = _load_schema('schema-event.json')
SEARCH_SCHEMA = _load_schema('schema-search.json')


def _validate_json(schema, obj):
    # required_by_default=False and blank_by_default=True for
    # JSON Schema draft 3 semantics
    validictory.validate(obj, schema, required_by_default=False,
            blank_by_default=True)


def _make_object_json(obj, url_factory):
    '''Convert an object attribute dict into a dict suitable for JSON
    encoding.  url_factory(k) is a function to convert an attribute name
    into a URL.'''
    result = {}
    for k, v in obj.iteritems():
        data = None
        # Inline known attribute types that can be represented in JSON
        if k.endswith('.int'):
            data = IntegerAttributeCodec().decode(v)
        elif k.endswith('.double'):
            data = DoubleAttributeCodec().decode(v)
        elif k.endswith('.patches'):
            distance, patches = PatchesAttributeCodec().decode(v)
            data = {
                'distance': distance,
                'patches': [{
                    'x0': tl[0],
                    'y0': tl[1],
                    'x1': br[0],
                    'y1': br[1]
                } for tl, br in patches],
            }
        else:
            # Treat remaining attributes as strings if: they don't have
            # a suffix representing a known binary type, they can be decoded
            # by the string codec (i.e., their last byte is 0), they are
            # valid UTF-8, and they are not the '' (object data) attribute.
            try:
                _base, suffix = k.rsplit('.', 1)
            except ValueError:
                suffix = None
            try:
                if k != '' and suffix not in ('jpeg', 'rgbimage', 'binary'):
                    data = StringAttributeCodec().decode(v).decode('UTF-8')
            except ValueError:
                pass

        if data is not None:
            result[k] = {
                'data': data,
            }
        else:
            result[k] = {
                'url': url_factory(k),
            }
    return result


class _BlasterRequestHandler(RequestHandler):
    @property
    def blob_cache(self):
        return self.application.blob_cache

    @property
    def search_cache(self):
        return self.application.search_cache

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
            _validate_json(SEARCH_SCHEMA, config)
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

        # Build filters
        blobs = {}  # blob -> itself  (for deduplication)
        def make_blob(obj):
            if obj is not None:
                blob = _BlasterBlob(obj['uri'], obj.get('sha256'))
                # Intern the blob
                return blobs.setdefault(blob, blob)
            else:
                return EmptyBlob()
        self.filters = [FilterSpec(
                    name=f['name'],
                    code=make_blob(f['code']),
                    arguments=f.get('arguments', []),
                    blob_argument=make_blob(f.get('blob')),
                    dependencies=f.get('dependencies', []),
                    min_score=f.get('min_score', float('-inf')),
                    max_score=f.get('max_score', float('inf'))
                ) for f in config['filters']]
        self.blobs = blobs.values()

    @gen.engine
    def fetch_blobs(self, blob_cache, callback=None):
        yield [gen.Task(blob.fetch, blob_cache) for blob in self.blobs]
        if callback is not None:
            callback()

    def make_search(self, **kwargs):
        return DiamondSearch(self.cookies, self.filters, **kwargs)


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
        search_key = self.search_cache.put_search(spec)

        # Return result
        self.set_status(204)
        self.set_header('Location', '/search')
        self.set_header('X-Search-Key', search_key)
        self.finish()


class PostBlobHandler(_BlasterRequestHandler):
    def post(self):
        sig = self.blob_cache.add(self.request.body)
        self.set_header('Location', '%s:%s' % (CACHE_URN_SCHEME, sig))
        self.set_status(204)


class AttributeHandler(_BlasterRequestHandler):
    def get(self, search_key, object_key, attr_name):
        try:
            data = self.search_cache.get_object_attribute(search_key,
                    object_key, attr_name)
        except KeyError:
            raise HTTPError(404, 'Not found')

        if attr_name.endswith('.jpeg'):
            self.set_header('Content-Type', 'image/jpeg')
        else:
            self.set_header('Content-Type', 'application/octet-stream')

        self.write(data)


class ResultsHandler(_BlasterRequestHandler):
    def get(self):
        if options.enable_testui:
            self.render('testui/results.html')
        else:
            raise HTTPError(403, 'Forbidden')


class _StructuredSocketConnection(SockJSConnection):
    @classmethod
    def event(cls, func):
        '''Decorator specifying that this function is an event handler.'''
        func.event_handler = True
        return func

    # pylint is confused by msg.get()
    # pylint: disable=E1103
    def on_message(self, data):
        try:
            msg = json.loads(data)
            _validate_json(EVENT_SCHEMA, msg)
        except ValueError, e:
            raise HTTPError(400, str(e))
        event = msg['event']
        try:
            handler = getattr(self, event)
        except AttributeError:
            _log.warning('Unknown event type %s' % event)
            return
        if not getattr(handler, 'event_handler', False):
            _log.warning('Event %s has invalid handler' % event)
            return
        handler(**msg.get('data', {}))
    # pylint: enable=E1103

    def emit(self, event, **args):
        self.send(json.dumps({
            'event': event,
            'data': args,
        }))


class SearchConnection(_StructuredSocketConnection):
    def __init__(self, *args, **kwargs):
        _StructuredSocketConnection.__init__(self, *args, **kwargs)
        self._search = None
        self._search_key = None

    @property
    def search_cache(self):
        return self.session.handler.application.search_cache

    def reverse_url(self, *args, **kwargs):
        return self.session.handler.application.reverse_url(*args, **kwargs)

    @_StructuredSocketConnection.event
    @gen.engine
    def start(self, search_key):
        # Sanity checks
        if self._search is not None:
            raise HTTPError(400, 'Search already started')

        # Load the search spec
        try:
            search_spec = self.search_cache.get_search(search_key)
        except KeyError:
            raise HTTPError(400, 'Invalid search key')
        except SearchCacheLoadError:
            raise HTTPError(400, 'Corrupt search key')

        # Start the search
        print 'start', search_key
        self._search_key = search_key
        self._search = search_spec.make_search(
            object_callback=self._result,
            finished_callback=self._finished,
            close_callback=self._closed,
        )
        search_id = yield gen.Task(self._search.start)

        # Return search ID to client
        self.emit('search_started', search_id=search_id)

        # Start statistics coroutine
        self._stats_coroutine()

    @gen.engine
    def _stats_coroutine(self):
        '''Statistics coroutine.'''
        while self._search is not None:
            yield gen.Task(self._send_stats)
            yield gen.Task(IOLoop.instance().add_timeout, STATS_INTERVAL)

    @gen.engine
    def _send_stats(self, callback=None):
        '''Fetch stats and send them to the client.'''
        stats = yield gen.Task(self._search.get_stats)
        self.emit('statistics', **stats)
        if callback is not None:
            callback()

    def _result(self, obj):
        '''Blast channel result.'''
        object_key = self.search_cache.put_search_result(self._search_key,
                obj['_ObjectID'], obj)
        def factory(k):
            return self.reverse_url('attribute', self._search_key,
                    object_key, k)
        result = _make_object_json(obj, factory)
        self.emit('result', **result)

    @gen.engine
    def _finished(self):
        '''Search has completed.'''
        # Send final stats
        yield gen.Task(self._send_stats)
        self.emit('search_complete')
        self._search.close()

    def _closed(self):
        '''Search closed.'''
        # Close the SockJS connection
        self.close()

    @_StructuredSocketConnection.event
    def pause(self):
        if self._search is None:
            raise HTTPError(400, 'Search not yet started')
        self._search.pause()

    @_StructuredSocketConnection.event
    def resume(self):
        if self._search is None:
            raise HTTPError(400, 'Search not yet started')
        self._search.resume()

    def on_close(self):
        '''SockJS connection closed.'''
        # Close the Diamond connection
        if self._search is not None:
            print 'close'
            search = self._search
            self._search = None
            search.close()
