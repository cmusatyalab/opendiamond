#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Scope list retrieval, parsing, and iteration.'''

from __future__ import with_statement
from future import standard_library
standard_library.install_aliases()
from builtins import next
from builtins import object
import logging
import urllib.request, urllib.error, urllib.parse
from urllib.parse import urljoin
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
    # pylint: disable=invalid-name
    def startElement(self, name, attrs):
        if name == 'objectlist':
            # count is optional
            count = attrs.get('count')
            if count is not None:
                self.count += int(count)
        elif name == 'count':
            self.count += int(attrs['adjust'])
        elif name == 'object':
            self.pending_objects.append(dict(attrs))
            # pylint: enable=invalid-name


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

    def __next__(self):
        '''Return the next Object.'''
        with self._lock:
            pending_object, scope_url = next(self._generator)

        id = pending_object.pop('id', None)
        src = pending_object.pop('src', None)
        meta = pending_object.pop('meta', None)

        # Allow 'src' and 'meta' be missing
        # If so, they should be loaded later in ObjectLoader        
        if src:
            src = urljoin(scope_url, src)
        if meta:
            meta = urljoin(scope_url, meta)

        # 'src' is the fallback value for 'id' for backward
        # compatibility (f.i. scopelists from Algum)
        # id can't be None eventually
        if id:
            id = urljoin(scope_url, id)
        elif src:
            id = src
        else:
            _log.error('An object cannot have none id and none src at the same time.')

        # if result cache is not enabled, no need to keep signatures in objects
        new_obj = Object(self.server_id,
                            id,
                            src=src,
                            meta=meta,
                            compute_signature=(self._config.cache_server is not None))

        # use the remaining attrs as normal object attributes
        for k, v in pending_object.items():
            new_obj[k] = v + '\0'

        return new_obj

    def _generator_func(self):
        # Build URL opener
        handlers = []
        if self._config.http_proxy is not None:
            handlers.append(urllib.request.ProxyHandler({
                'http': self._config.http_proxy,
                'https': self._config.http_proxy,
            }))
        opener = urllib.request.build_opener(*handlers)
        opener.addheaders = [('User-Agent', self._config.user_agent)]
        # Build XML parser
        parser = make_parser()
        parser.setContentHandler(self._handler)
        # Walk scope URLs
        count = 0
        for scope_url in (url for cookie in self.cookies for url in cookie):
            scope_url = urljoin(BASE_URL, scope_url)
            try:
                # We use urllib2 here because different parts of a single
                # HTTP response will be handled from different threads.
                # pycurl does not support this.
                fh = opener.open(scope_url)
                # Read the scope list in 4 KB chunks
                while True:
                    buf = fh.read(4096)
                    if not buf:
                        break
                    parser.feed(buf)
                    while self._handler.pending_objects:
                        pending_object = self._handler.pending_objects.pop(0)
                        count += 1
                        yield (pending_object, scope_url)

            except urllib.error.URLError as e:
                _log.warning('Fetching %s: %s', scope_url, e)
            except SAXParseException as e:
                _log.warning('Parsing %s: %s', scope_url, e)
            finally:
                try:
                    parser.close()
                except SAXParseException:
                    # Received malformed XML, such as XML with missing
                    # closing tags.  This is likely caused by a
                    # prematurely-terminated connection.
                    _log.warning('Parsing %s: incomplete scope list',
                                 scope_url)
                parser.reset()
        # Log successful completion
        _log.info('End of scope list (%d)', count)

    def get_count(self):
        '''Return our current understanding of the number of objects in
        scope.'''
        with self._lock:
            return self._handler.count
