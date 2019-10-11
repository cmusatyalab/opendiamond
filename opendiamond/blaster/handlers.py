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

from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import object
from io import StringIO
from datetime import timedelta
from hashlib import sha256
import logging
import os
import time
from urllib.parse import urljoin, urlparse

import magic
import PIL.Image
import PIL.ImageColor
import simplejson as json
from sockjs.tornado import SockJSConnection
from tornado.curl_httpclient import CurlAsyncHTTPClient as AsyncHTTPClient
from tornado import gen
from tornado.ioloop import IOLoop
from tornado.options import define, options
from tornado.web import asynchronous, RequestHandler, HTTPError

import opendiamond
from opendiamond.attributes import (
    StringAttributeCodec, IntegerAttributeCodec, DoubleAttributeCodec,
    RGBImageAttributeCodec, PatchesAttributeCodec)
from opendiamond.blaster.cache import SearchCacheLoadError
from opendiamond.blaster.json import (
    SearchConfig, SearchConfigResult, EvaluateRequest, ResultObject,
    ClientToServerEvent, ServerToClientEvent)
from opendiamond.blaster.search import (
    Blob, EmptyBlob, DiamondSearch, FilterSpec)
from opendiamond.helpers import connection_ok
from opendiamond.protocol import DiamondRPCCookieExpired
from opendiamond.rpc import ConnectionFailure, RPCError
from opendiamond.scope import ScopeCookie, ScopeError
from functools import reduce

CACHE_URN_SCHEME = 'blob'
STATS_INTERVAL = timedelta(milliseconds=1000)
PING_INTERVAL = timedelta(seconds=10)
CONNECTION_TIMEOUT = 30  # seconds

# HTTP method handlers have specific argument lists rather than
# (self, *args, **kwargs) as in the superclass.
# pylint: disable=arguments-differ

define('enable_testui', default=True,
       help='Enable the example user interface')
define('http_proxy', type=str, default=None,
       metavar='HOST:PORT', help='Use a proxy for HTTP client requests')

_log = logging.getLogger(__name__)


_magic = magic.open(magic.MAGIC_NONE)
_magic.setflags(getattr(magic, 'MAGIC_MIME_TYPE', 0x10))
_magic.load()


# Be strict in what we send and liberal in what we accept
_search_schema = SearchConfig(strict=False)
_search_result_schema = SearchConfigResult(strict=True)
_evaluate_request_schema = EvaluateRequest(strict=False)
_result_object_schema = ResultObject(strict=True)
_c2s_event_schema = ClientToServerEvent(strict=False)
_s2c_event_schema = ServerToClientEvent(strict=True)


def _make_object_json(application, search_key, object_key, obj):
    '''Convert an object attribute dict into a dict suitable for JSON
    encoding.'''
    result = {}
    for k, v in obj.items():
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
                if k != '' and suffix not in ('jpeg', 'png', 'rgbimage',
                                              'binary'):
                    data = StringAttributeCodec().decode(v).decode('UTF-8')
            except ValueError:
                pass

        if data is not None:
            result[k] = {
                'data': data,
            }
        else:
            result[k] = {
                'raw_url': application.reverse_url(
                    'attribute-raw', search_key, object_key, k),
                'image_url': application.reverse_url(
                    'attribute-image', search_key, object_key, k),
            }
    result['_ResultURL'] = {
        'data': application.reverse_url('result', search_key, object_key),
    }
    _result_object_schema.validate(result)
    return result


def _restricted(func):
    '''Decorator that returns 403 if the remote IP is forbidden by
    TCP Wrappers.'''
    def wrapper(self, *args, **kwargs):
        if not connection_ok('blaster', self.request.remote_ip):
            raise HTTPError(403, 'Forbidden')
        return func(self, *args, **kwargs)
    return wrapper


# Method 'data_received' is abstract in class 'RequestHandler' but is not
# overridden (abstract-method)
# pylint: disable=abstract-method
class _BlasterRequestHandler(RequestHandler):
    @property
    def blob_cache(self):
        return self.application.blob_cache

    @property
    def search_cache(self):
        return self.application.search_cache

    @property
    def request_content_type(self):
        return self.request.headers['Content-Type'].split(';')[0]

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
                response = yield gen.Task(
                    client.fetch, self.uri,
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


class _SearchSpec(object):
    def __init__(self, data):
        # Load JSON
        try:
            config = json.loads(data)
            _search_schema.validate(config)
        except ValueError as e:
            raise HTTPError(400, str(e))

        # Build cookies
        # Assume each "cookie" may actually be a megacookie
        try:
            self.cookies = [ScopeCookie.parse(c) for mc in config['cookies']
                            for c in ScopeCookie.split(mc)]
        except ScopeError as e:
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
            return EmptyBlob()

        self.filters = [
            FilterSpec(
                name=f['name'],
                code=make_blob(f['code']),
                arguments=f.get('arguments', []),
                blob_argument=make_blob(f.get('blob')),
                dependencies=f.get('dependencies', []),
                min_score=f.get('min_score', float('-inf')),
                max_score=f.get('max_score', float('inf'))
            ) for f in config['filters']]
        self.blobs = list(blobs.values())

    @gen.engine
    def fetch_blobs(self, blob_cache, callback=None):
        yield [gen.Task(blob.fetch, blob_cache) for blob in self.blobs]
        if callback is not None:
            callback()

    @property
    def expires(self):
        return reduce(lambda a, b: a if a < b else b,
                      [c.expires for c in self.cookies])

    def make_search(self, **kwargs):
        return DiamondSearch(self.cookies, self.filters, **kwargs)


class SearchHandler(_BlasterRequestHandler):
    @_restricted
    def get(self):
        if options.enable_testui:
            self.redirect(self.reverse_url('ui-search'))
        else:
            raise HTTPError(405, 'Method not allowed')

    @asynchronous
    @gen.engine
    @_restricted
    def post(self):
        # Build search spec
        if self.request_content_type != 'application/json':
            raise HTTPError(415, 'Content type must be application/json')
        spec = _SearchSpec(self.request.body)
        yield gen.Task(spec.fetch_blobs, self.blob_cache)

        # Store it
        search_key = self.search_cache.put_search(spec, spec.expires)

        # Return result
        result = {
            'evaluate_url': self.reverse_url('evaluate', search_key),
            'socket_url': urljoin(options.baseurl, '/search'),
            'search_key': search_key,
        }
        _search_result_schema.validate(result)
        self.set_status(200)
        self.set_header('Content-Type', 'application/json')
        self.write(json.dumps(result))
        self.finish()


class PostBlobHandler(_BlasterRequestHandler):
    @_restricted
    def post(self):
        sig = self.blob_cache.add(self.request.body)
        self.set_header('Location', '%s:%s' % (CACHE_URN_SCHEME, sig))
        self.set_status(204)


class EvaluateHandler(_BlasterRequestHandler):
    def initialize(self):
        self._running = False

    @asynchronous
    @gen.engine
    @_restricted
    def post(self, search_key):
        # Load the search spec
        try:
            search_spec = self.search_cache.get_search(search_key)
        except KeyError:
            raise HTTPError(404)
        except SearchCacheLoadError:
            raise HTTPError(400, 'Corrupt search key')

        # Load JSON request
        if self.request_content_type != 'application/json':
            raise HTTPError(415, 'Content type must be application/json')
        try:
            request = json.loads(self.request.body)
            _evaluate_request_schema.validate(request)
        except ValueError as e:
            raise HTTPError(400, str(e))

        # Load the object data
        req_obj = request['object']
        blob = _BlasterBlob(req_obj['uri'], req_obj.get('sha256'))
        yield gen.Task(blob.fetch, self.blob_cache)

        # Reexecute
        _log.info('Evaluating search %s on object %s',
                  search_key, blob.sha256)
        self._running = True
        search = search_spec.make_search(close_callback=self._closed)
        try:
            obj = yield gen.Task(search.evaluate, blob)
        except DiamondRPCCookieExpired:
            raise HTTPError(400, 'Scope cookie expired')
        except (RPCError, ConnectionFailure):
            _log.exception('evaluate failed')
            raise HTTPError(400, 'Evaluation failed')
        finally:
            self._running = False
            search.close()

        # Store object in cache
        object_key = self.search_cache.put_search_result(
            search_key, obj['_ObjectID'], obj)

        # Return result
        result = _make_object_json(self.application, search_key,
                                   object_key, obj)
        self.set_status(200)
        self.set_header('Content-Type', 'application/json')
        self.write(json.dumps(result))
        self.finish()

    def _closed(self):
        '''Reexecution connection closed.'''
        if self._running:
            raise HTTPError(400, 'Evaluation failed')


class ResultHandler(_BlasterRequestHandler):
    def get(self, search_key, object_key):
        try:
            obj = self.search_cache.get_search_result(
                search_key, object_key)
        except KeyError:
            raise HTTPError(404, 'Not found')
        result = _make_object_json(self.application, search_key,
                                   object_key, obj)
        objdata = json.dumps(result)
        jsonp = self.get_argument('jsonp', None)
        # Allow cross-domain access to result data
        self.set_header('Access-Control-Allow-Origin', '*')
        # Allow caching for one week
        self.set_header('Cache-Control', 'max-age=604800')
        if jsonp is not None:
            self.set_header('Content-Type', 'application/javascript')
            objdata = '%s(%s);' % (jsonp, objdata)
        else:
            self.set_header('Content-Type', 'application/json')
        self.write(objdata)


class AttributeHandler(_BlasterRequestHandler):
    def initialize(self, transcode=False):
        self._transcode = transcode

    def get(self, search_key, object_key, attr_name):
        try:
            data = self.search_cache.get_search_result(
                search_key, object_key)[attr_name]
        except KeyError:
            raise HTTPError(404, 'Not found')

        tint = self.get_argument('tint', None)
        if tint is not None:
            try:
                tint = PIL.ImageColor.getrgb('#' + tint)
            except ValueError:
                raise HTTPError(400, 'Invalid tint value')

        if attr_name.endswith('.jpeg'):
            mime = 'image/jpeg'
        else:
            mime = _magic.buffer(data)

        if self._transcode and (tint is not None or
                                mime not in ('image/jpeg', 'image/png')):
            try:
                if attr_name.endswith('.rgbimage'):
                    img = RGBImageAttributeCodec().decode(data)
                else:
                    img = PIL.Image.open(StringIO(data))
                if tint is not None:
                    if img.mode != 'L':
                        img = img.convert('L')
                    alpha = img
                    img = PIL.Image.new('RGB', img.size, tint)
                    img.putalpha(alpha)
                buf = StringIO()
                img.save(buf, 'PNG')
                data = buf.getvalue()
                mime = 'image/png'
            except IOError:
                # Couldn't parse image
                pass

        self.set_header('Content-Type', mime)
        # Allow cross-domain access to image data via canvas
        self.set_header('Access-Control-Allow-Origin', '*')
        # Allow caching for one week
        self.set_header('Cache-Control', 'max-age=604800')
        self.write(data)


class UIHandler(_BlasterRequestHandler):
    def initialize(self, template):
        self._template = template

    @_restricted
    def get(self):
        if options.enable_testui:
            self.render(os.path.join('testui', self._template))
        else:
            raise HTTPError(403, 'Forbidden')


class _StructuredSocketConnection(SockJSConnection):
    def __init__(self, c2s_schema, s2c_schema, *args, **kwargs):
        self._c2s_schema = c2s_schema
        self._s2c_schema = s2c_schema
        SockJSConnection.__init__(self, *args, **kwargs)

    @classmethod
    def event(cls, func):
        '''Decorator specifying that this function is an event handler.'''
        func.event_handler = True
        return func

    # pylint is confused by msg.get()
    # pylint: disable=maybe-no-member
    def on_message(self, data):
        try:
            msg = json.loads(data)
            self._c2s_schema.validate(msg)
        except ValueError as e:
            self.error(str(e))
            return
        event = msg['event']
        try:
            handler = getattr(self, event)
            if not getattr(handler, 'event_handler', False):
                raise AttributeError()
        except AttributeError:
            self.error('Unknown event type %s' % event)
            return
        handler(**msg.get('data', {}))
    # pylint: enable=maybe-no-member

    def emit(self, event, **args):
        data = {
            'event': event,
            'data': args,
        }
        self._s2c_schema.validate(data)
        self.send(json.dumps(data))

    def error(self, message):
        raise NotImplementedError()


class SearchConnection(_StructuredSocketConnection):
    def __init__(self, *args, **kwargs):
        _StructuredSocketConnection.__init__(
            self, _c2s_event_schema, _s2c_event_schema, *args, **kwargs)
        self._search = None
        self._search_key = None
        self._last_pong = None

    @property
    def application(self):
        return self.session.server.application

    @property
    def search_cache(self):
        return self.application.search_cache

    @_StructuredSocketConnection.event
    @gen.engine
    def start(self, search_key):
        # Sanity checks
        if self._search is not None:
            self.error('Search already started')
            return

        # Load the search spec
        try:
            search_spec = self.search_cache.get_search(search_key)
        except KeyError:
            self.error('Invalid search key')
            return
        except SearchCacheLoadError:
            self.error('Corrupt search key')
            return

        # Create the search
        self._search_key = search_key
        self._search = search_spec.make_search(
            object_callback=self._result,
            finished_callback=self._finished,
            close_callback=self._closed,
        )

        # Now that self._search is set, start pinging the client
        self._keepalive_coroutine()

        # Start the search
        _log.info('Starting search %s', search_key)
        try:
            search_id = yield gen.Task(self._search.start)
        except DiamondRPCCookieExpired:
            self.error('Scope cookie expired')
            self.close()
            return
        except (RPCError, ConnectionFailure):
            _log.exception('start failed')
            self.error('Could not start search')
            self.close()
            return

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
    def _keepalive_coroutine(self):
        '''SockJS times out polling connections quickly, but websocket
        connections don't time out until TCP does, which can take 15 minutes.
        Ping the client periodically, and if it doesn't respond for a while,
        close the connection.'''
        self._last_pong = time.time()
        while self._search is not None:
            if time.time() > self._last_pong + CONNECTION_TIMEOUT:
                _log.info('Client connection timed out')
                self.close()
                return
            self.emit('ping')
            yield gen.Task(IOLoop.instance().add_timeout, PING_INTERVAL)

    @gen.engine
    def _send_stats(self, callback=None):
        '''Fetch stats and send them to the client.'''
        stats = yield gen.Task(self._search.get_stats)
        self.emit('statistics', **stats)
        if callback is not None:
            callback()

    def _result(self, obj):
        '''Blast channel result.'''
        object_key = self.search_cache.put_search_result(
            self._search_key, obj['_ObjectID'], obj)
        result = _make_object_json(self.application, self._search_key,
                                   object_key, obj)
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
        _log.info('Search %s terminated', self._search_key)
        # Close the SockJS connection
        self.close()

    @_StructuredSocketConnection.event
    def pause(self):
        if self._search is None:
            self.error('Search not yet started')
            return
        self._search.pause()

    @_StructuredSocketConnection.event
    def resume(self):
        if self._search is None:
            self.error('Search not yet started')
            return
        self._search.resume()

    @_StructuredSocketConnection.event
    def pong(self):
        self._last_pong = time.time()

    def error(self, message):
        self.emit('error', message=message)

    def on_close(self):
        '''SockJS connection closed.'''
        # Close the Diamond connection
        if self._search is not None:
            search = self._search
            self._search = None
            search.close()
