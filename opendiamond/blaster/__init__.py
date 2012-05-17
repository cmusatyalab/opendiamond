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

import logging
import os
from sockjs.tornado import SockJSRouter
import threading
import time
import tornado.ioloop
from tornado.options import define, options
import tornado.web
from tornado.web import url

from opendiamond.blobcache import BlobCache
from opendiamond.blaster.cache import SearchCache
from opendiamond.blaster.handlers import (SearchHandler, PostBlobHandler,
        ResultHandler, AttributeHandler, UIHandler, SearchConnection)

define('blob_cache_dir',
        default=os.path.expanduser('~/.diamond/blob-cache-json'),
        metavar='DIR', help='Cache directory for binary objects')
define('search_cache_dir',
        default=os.path.expanduser('~/.diamond/search-cache-json'),
        metavar='DIR', help='Cache directory for search definitions')

_log = logging.getLogger(__name__)


class JSONBlaster(tornado.web.Application):
    handlers = (
        (r'/$', SearchHandler),
        (r'/blob$', PostBlobHandler),
        url(r'/result/([0-9a-f]{64})/([0-9a-f]{64})$',
                ResultHandler, name='result'),
        url(r'/result/([0-9a-f]{64})/([0-9a-f]{64})/raw/(.*)$',
                AttributeHandler, name='attribute-raw'),
        url(r'/result/([0-9a-f]{64})/([0-9a-f]{64})/image/(.*)$',
                AttributeHandler, kwargs={'transcode': True},
                name='attribute-image'),
        url(r'/ui$', UIHandler, name='ui-search',
                kwargs={'template': 'search.html'}),
        url(r'/ui/results$', UIHandler, name='ui-results',
                kwargs={'template': 'results.html'}),
    )

    app_settings = {
        'static_path': os.path.join(os.path.dirname(__file__), 'static'),
        'template_path': os.path.join(os.path.dirname(__file__), 'templates'),
    }

    sockjs_settings = {
        'sockjs_url': '/static/sockjs.js',
    }

    cache_prune_interval = 3600 # seconds
    # The blob cache is only used as a holding area for blobs that will soon
    # be added to a search, so cache objects don't need a long lifetime.
    blob_cache_days = 1

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

        self._pruner = threading.Thread(target=self._prune_cache_thread,
                name='prune-cache')
        self._pruner.daemon = True
        self._pruner.start()

    # We don't want to abort the pruning thread on an exception
    # pylint: disable=W0703
    def _prune_cache_thread(self):
        '''Runs as a separate Python thread; cannot interact with Tornado
        state.'''
        while True:
            try:
                BlobCache.prune(self.blob_cache.basedir, self.blob_cache_days)
            except Exception:
                _log.exception('Pruning blob cache')
            try:
                self.search_cache.prune()
            except Exception:
                _log.exception('Pruning search cache')
            time.sleep(self.cache_prune_interval)
    # pylint: enable=W0703
