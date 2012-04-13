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

import json
from tornado.options import define, options
from tornado.web import RequestHandler, HTTPError

# HTTP method handlers have specific argument lists rather than
# (self, *args, **kwargs) as in the superclass.
# pylint: disable=W0221

define('enable_testui', default=True,
        help='Enable the example user interface')


class _BlasterRequestHandler(RequestHandler):
    @property
    def blob_cache(self):
        return self.application.blob_cache


class SearchHandler(_BlasterRequestHandler):
    def get(self):
        if options.enable_testui:
            self.render('testui/search.html')
        else:
            raise HTTPError(405)

    def post(self):
        if self.request.headers['Content-Type'] != 'application/json':
            raise HTTPError(415)
        try:
            config = json.loads(self.request.body)
        except ValueError:
            raise HTTPError(400)
        self.write(json.dumps(config, indent=2))


class PostBlobHandler(_BlasterRequestHandler):
    def post(self):
        self.blob_cache.add(self.request.body)
        self.set_status(204)


class BlobHandler(_BlasterRequestHandler):
    def head(self, sha256):
        if sha256 not in self.blob_cache:
            raise HTTPError(404)
