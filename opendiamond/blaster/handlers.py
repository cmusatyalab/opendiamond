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
from tornado.options import define, options
from tornado.web import RequestHandler, HTTPError
import validictory

CACHE_URN_SCHEME = 'blob'

# HTTP method handlers have specific argument lists rather than
# (self, *args, **kwargs) as in the superclass.
# pylint: disable=W0221

define('enable_testui', default=True,
        help='Enable the example user interface')


def _load_schema(name):
    with open(os.path.join(os.path.dirname(__file__), name)) as fh:
        return json.load(fh)
SEARCH_SCHEMA = _load_schema('schema-search.json')


class _BlasterRequestHandler(RequestHandler):
    @property
    def blob_cache(self):
        return self.application.blob_cache

    def write_error(self, code, **kwargs):
        exc_type, exc_value, exc_tb = kwargs.get('exc_info', [None] * 3)
        if exc_type is not None and issubclass(exc_type, HTTPError):
            self.set_header('Content-Type', 'text/plain')
            if exc_value.log_message:
                self.write(exc_value.log_message + '\n')
        else:
            RequestHandler.write_error(self, code, **kwargs)


class SearchHandler(_BlasterRequestHandler):
    def get(self):
        if options.enable_testui:
            self.render('testui/search.html')
        else:
            raise HTTPError(405, 'Method not allowed')

    def post(self):
        if self.request.headers['Content-Type'] != 'application/json':
            raise HTTPError(415, 'Content type must be application/json')
        try:
            config = json.loads(self.request.body)
            # required_by_default=False and blank_by_default=True for
            # JSON Schema draft 3 semantics
            validictory.validate(config, SEARCH_SCHEMA,
                    required_by_default=False, blank_by_default=True)
        except ValueError, e:
            raise HTTPError(400, str(e))
        self.write(json.dumps(config, indent=2))


class PostBlobHandler(_BlasterRequestHandler):
    def post(self):
        sig = self.blob_cache.add(self.request.body)
        self.set_header('Location', '%s:%s' % (CACHE_URN_SCHEME, sig))
        self.set_status(204)
