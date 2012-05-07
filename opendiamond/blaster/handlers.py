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
from urlparse import urlparse
from tornado.curl_httpclient import CurlAsyncHTTPClient as AsyncHTTPClient
from tornado import gen
from tornado.options import define, options
from tornado.web import asynchronous, RequestHandler, HTTPError
import validictory

import opendiamond
from opendiamond.blaster.search import Blob, EmptyBlob
from opendiamond.helpers import sha256

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


class SearchHandler(_BlasterRequestHandler):
    def get(self):
        if options.enable_testui:
            self.render('testui/search.html')
        else:
            raise HTTPError(405, 'Method not allowed')

    @asynchronous
    @gen.engine
    def post(self):
        if self.request.headers['Content-Type'] != 'application/json':
            raise HTTPError(415, 'Content type must be application/json')

        # Load JSON
        try:
            config = json.loads(self.request.body)
            # required_by_default=False and blank_by_default=True for
            # JSON Schema draft 3 semantics
            validictory.validate(config, SEARCH_SCHEMA,
                    required_by_default=False, blank_by_default=True)
        except ValueError, e:
            raise HTTPError(400, str(e))

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

        # Fetch blobs
        yield [gen.Task(blob.fetch, self.blob_cache)
                for blob in blobs.values()]

        # Dump JSON
        self.write(json.dumps(config, indent=2))
        self.finish()


class PostBlobHandler(_BlasterRequestHandler):
    def post(self):
        sig = self.blob_cache.add(self.request.body)
        self.set_header('Location', '%s:%s' % (CACHE_URN_SCHEME, sig))
        self.set_status(204)
