#
#  The OpenDiamond Platform for Interactive Search
#  Version 4
#
#  Copyright (c) 2009 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#
#
# Proxy which returns a subset of the scopelist from a remote data retriever
#

from wsgiref.util import shift_path_info
from urllib import unquote_plus
from urllib2 import urlopen
from urlparse import urljoin
from xml.etree.ElementTree import iterparse

def scope_app(environ, start_response):
    root = shift_path_info(environ)
    index, count = map(int, root.split('of'))

    url = 'http:/' + environ['PATH_INFO']
    if environ.has_key('QUERY_STRING'):
	url += '?' + environ['QUERY_STRING']

    start_response("200 OK", [('Content-Type', "text/xml")])
    return parseScope(url, index, count)

def parseScope(base_url, index, count):
    index = index - 1
    seen = 0
    obj = urlopen(base_url)

    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    yield '<objectlist>\n'

    root = None
    for ev, el in iterparse(obj, events=("start","end")):
	if ev == 'start' and root is None:
	    root = el
	if ev == 'end' and el.tag == 'object':
	    if seen % count == index:
		src = urljoin(base_url, el.attrib['src'])
		yield '<count adjust="1"/><object src="%s"/>\n' % src
	    seen += 1
	    root.clear()

    yield '</objectlist>\n'
    obj.close()

