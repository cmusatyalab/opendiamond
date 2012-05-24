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
import tornado.ioloop
from tornado.options import define, options
import tornado.web

from opendiamond.blobcache import BlobCache
from opendiamond.blaster.handlers import (SearchHandler, PostBlobHandler)

define('cache_dir', default=os.path.expanduser('~/.diamond/cache-json'),
        metavar='DIR', help='Cache directory for binary objects')


class JSONBlaster(tornado.web.Application):
    handlers = (
        (r'/$', SearchHandler),
        (r'/blob$', PostBlobHandler),
    )

    app_settings = {
        'static_path': os.path.join(os.path.dirname(__file__), 'static'),
        'template_path': os.path.join(os.path.dirname(__file__), 'templates'),
    }

    def __init__(self, **kwargs):
        settings = dict(self.app_settings)
        settings.update(kwargs)
        tornado.web.Application.__init__(self, self.handlers, **settings)
        if not os.path.isdir(options.cache_dir):
            os.makedirs(options.cache_dir, 0700)
        self.blob_cache = BlobCache(options.cache_dir)
