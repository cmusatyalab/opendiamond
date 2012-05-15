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

'''JSON Blaster web application.'''

import os
from sockjs.tornado import SockJSRouter
import tornado.ioloop
from tornado.options import define, options
import tornado.web
from tornado.web import url

from opendiamond.blobcache import BlobCache
from opendiamond.blaster.cache import SearchCache
from opendiamond.blaster.handlers import (SearchHandler, PostBlobHandler,
        AttributeHandler, ResultsHandler, SearchConnection)

define('blob_cache_dir',
        default=os.path.expanduser('~/.diamond/blob-cache-json'),
        metavar='DIR', help='Cache directory for binary objects')
define('search_cache_dir',
        default=os.path.expanduser('~/.diamond/search-cache-json'),
        metavar='DIR', help='Cache directory for search definitions')


class JSONBlaster(tornado.web.Application):
    handlers = (
        (r'/$', SearchHandler),
        (r'/blob$', PostBlobHandler),
        url(r'/attribute/([0-9a-f]{64})/([0-9a-f]{64})/(.+)$',
                AttributeHandler, name='attribute'),
        (r'/results$', ResultsHandler),
    )

    app_settings = {
        'static_path': os.path.join(os.path.dirname(__file__), 'static'),
        'template_path': os.path.join(os.path.dirname(__file__), 'templates'),
    }

    sockjs_settings = {
        'sockjs_url': '/static/sockjs.js',
    }

    def __init__(self, **kwargs):
        handlers = list(self.handlers)
        SockJSRouter(SearchConnection, '/search',
                self.sockjs_settings).apply_routes(handlers)
        settings = dict(self.app_settings)
        settings.update(kwargs)
        tornado.web.Application.__init__(self, handlers, **settings)

        if not os.path.isdir(options.blob_cache_dir):
            os.makedirs(options.blob_cache_dir, 0700)
        self.blob_cache = BlobCache(options.blob_cache_dir, 'sha256')

        self.search_cache = SearchCache(options.search_cache_dir)
