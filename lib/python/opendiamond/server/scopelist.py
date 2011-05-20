#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2002-2007 Intel Corporation
#  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
#  Copyright (c) 2006-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from __future__ import with_statement
from urllib2 import urlopen
from urlparse import urljoin
import threading
from xml.sax import make_parser
from xml.sax.handler import ContentHandler

from opendiamond.server.object_ import Object

class _ScopeListHandler(ContentHandler):
    def __init__(self):
        self.count = 0
        self.pending_objects = []

    def startElement(self, name, attrs):
        if name == 'objectlist':
            # count is optional
            count = attrs.get('count')
            if count is not None:
                self.count += int(count)
        elif name == 'count':
            self.count += int(attrs['adjust'])
        elif name == 'object':
            self.pending_objects.append(attrs['src'])


class ScopeListLoader(object):
    def __init__(self, server_id, cookies):
        self.server_id = server_id
        self.cookies = cookies
        self._lock = threading.Lock()
        self._handler = _ScopeListHandler()
        self._generator = self._generator_func()

    def __iter__(self):
        return self

    def next(self):
        with self._lock:
            return self._generator.next()

    def _generator_func(self):
        parser = make_parser()
        parser.setContentHandler(self._handler)
        for cookie in self.cookies:
            for scope_url in cookie:
                fh = urlopen(scope_url)
                # Read the scope list in 4 KB chunks
                while True:
                    buf = fh.read(4096)
                    if len(buf) == 0:
                        break
                    parser.feed(buf)
                    while len(self._handler.pending_objects) > 0:
                        url = self._handler.pending_objects.pop(0)
                        yield Object(self.server_id, urljoin(scope_url, url))
                parser.close()
                parser.reset()

    def get_count(self):
        with self._lock:
            return self._handler.count
