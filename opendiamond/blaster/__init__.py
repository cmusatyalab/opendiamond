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
from tornadio2 import SocketConnection, TornadioRouter
import tornado.ioloop
from tornado.options import define, options
import tornado.web

from opendiamond.blobcache import BlobCache
from opendiamond.blaster.handlers import (SearchHandler, PostBlobHandler,
        ResultsHandler, SearchConnection)

define('blob_cache_dir',
        default=os.path.expanduser('~/.diamond/blob-cache-json'),
        metavar='DIR', help='Cache directory for binary objects')
define('search_cache_dir',
        default=os.path.expanduser('~/.diamond/search-cache-json'),
        metavar='DIR', help='Cache directory for search definitions')


class _JSONBlasterConnection(SocketConnection):
    __endpoints__ = {
        '/search': SearchConnection,
    }

    def on_message(self, _message):
        # Must be overridden; superclass is abstract
        raise tornado.web.HTTPError(400, 'Cannot send messages here')


class JSONBlaster(tornado.web.Application):
    handlers = (
        (r'/$', SearchHandler),
        (r'/blob$', PostBlobHandler),
        (r'/results$', ResultsHandler),
    )

    app_settings = {
        'enabled_protocols': ['websocket', 'xhr-polling', 'jsonp-polling'],
        'static_path': os.path.join(os.path.dirname(__file__), 'static'),
        'template_path': os.path.join(os.path.dirname(__file__), 'templates'),
    }

    def __init__(self, **kwargs):
        handlers = list(self.handlers)
        TornadioRouter(_JSONBlasterConnection).apply_routes(handlers)
        settings = dict(self.app_settings)
        settings.update(kwargs)
        tornado.web.Application.__init__(self, handlers, **settings)
        def make_cache(path):
            if not os.path.isdir(path):
                os.makedirs(path, 0700)
            return BlobCache(path, 'sha256')
        self.blob_cache = make_cache(options.blob_cache_dir)
        self.search_spec_cache = make_cache(options.search_cache_dir)
