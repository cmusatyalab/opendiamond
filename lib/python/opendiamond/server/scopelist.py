#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Scope list retrieval, parsing, and iteration.'''

from __future__ import with_statement
import logging
import urllib2
from urlparse import urljoin
import threading
from xml.sax import make_parser, SAXParseException
from xml.sax.handler import ContentHandler

from opendiamond.server.object_ import Object

BASE_URL = 'http://localhost:5873/'

_log = logging.getLogger(__name__)

class _ScopeListHandler(ContentHandler):
    '''Gatherer for results produced by incremental scope list parsing.'''

    def __init__(self):
        ContentHandler.__init__(self)
        self.count = 0
        self.pending_objects = []

    # We're overriding a method; we can't control its name
    # pylint: disable=C0103
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
    # pylint: enable=C0103


class ScopeListLoader(object):
    '''Iterator over the objects in the scope lists referenced by the scope
    cookies.'''

    def __init__(self, config, server_id, cookies):
        self.server_id = server_id
        self.cookies = cookies
        self._config = config
        self._lock = threading.Lock()
        self._handler = _ScopeListHandler()
        self._generator = self._generator_func()

    def __iter__(self):
        return self

    # Generators confuse pylint, pylint #20062
    # pylint: disable=E1101
    def next(self):
        '''Return the next Object.'''
        with self._lock:
            return self._generator.next()
    # pylint: enable=E1101

    def _generator_func(self):
        # Build URL opener
        handlers = []
        if self._config.http_proxy is not None:
            handlers.append(urllib2.ProxyHandler({
                'http': self._config.http_proxy,
                'https': self._config.http_proxy,
            }))
        opener = urllib2.build_opener(*handlers)
        opener.addheaders = [('User-Agent', self._config.user_agent)]
        # Build XML parser
        parser = make_parser()
        parser.setContentHandler(self._handler)
        # Walk scope URLs
        for cookie in self.cookies:
            for scope_url in cookie:
                scope_url = urljoin(BASE_URL, scope_url)
                try:
                    # We use urllib2 here because different parts of a single
                    # HTTP response will be handled from different threads.
                    # pycurl does not support this.
                    fh = opener.open(scope_url)
                    # Read the scope list in 4 KB chunks
                    while True:
                        buf = fh.read(4096)
                        if len(buf) == 0:
                            break
                        parser.feed(buf)
                        while len(self._handler.pending_objects) > 0:
                            url = self._handler.pending_objects.pop(0)
                            yield Object(self.server_id,
                                            urljoin(scope_url, url))
                except urllib2.URLError, e:
                    _log.warning('Fetching %s: %s', scope_url, e)
                except SAXParseException, e:
                    _log.warning('Parsing %s: %s', scope_url, e)
                finally:
                    try:
                        # Ignore well-formedness errors discovered at close
                        parser.close()
                    except SAXParseException:
                        pass
                    parser.reset()

    def get_count(self):
        '''Return our current understanding of the number of objects in
        scope.'''
        with self._lock:
            return self._handler.count
