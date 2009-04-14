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
# Helper functions for an OpenDiamond DataRetriever WSGI application
#

STYLE = False	# export XML stylesheet for web browsers

__all__ = ["guess_mime_type", "ScopelistWrapper", "DataRetriever", "run_server"]

# the following mime type guessing is from SimpleHTTPServer.py
import posixpath
import mimetypes
from wsgiref.util import FileWrapper, shift_path_info

if not mimetypes.inited:
    mimetypes.init()
extensions = mimetypes.types_map.copy()
extensions.update({ '': 'application/octet-stream' })

def guess_mime_type(path):
    base, ext = posixpath.splitext(path)
    if ext in extensions:
	return extensions[ext]
    ext = ext.lower()
    if ext in extensions:
	return extensions[ext]
    else:
	return extensions['']

def ScopelistWrapper(list, base_uri, nentries=0):
    yield '<?xml version="1.0" encoding="UTF-8" ?>'
    if STYLE:
	yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>'
    count = ''
    if nentries: count='count="%d"' % nentries
    yield '<objectlist %s>' % count
    for path in list:
	yield '<object src="%s/%s" />' % (base_uri, path.strip())
    if hasattr(list, 'close'):
	list.close()
    yield '</objectlist>'

# return xslt stylesheet which makes browsers show the scope list as thumbnails.
# guaranteed to bring chaos with any decent data set.
def scopelist_xsl(environ, start_response):
    content = """\
<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns="http://www.w3.org/1999/xhtml">
<xsl:output method="xml" indent="yes" encoding="UTF-8"/>
<xsl:template match="/objectlist">
    <html>
    <head><title>OpenDiamond Scope List</title></head>
    <body>
    <xsl:for-each select="object">
    <img height="64" width="64">
    <xsl:attribute name="src"><xsl:value-of select="@src"/></xsl:attribute>
    </img>
    </xsl:for-each>
    </body>
    </html>
</xsl:template>
</xsl:stylesheet>
"""
    start_response("200 OK", [
	("Content-Type", "text/xsl"),
	("Content-Length", str(len(content))),
    ])
    return [content]

# WSGI middleware that cleans up PATH_INFO, dispatches requests based on a
# dictionary of handlers and catches exceptions.
class DataRetriever:
    def __init__(self, handlers):
	self.handlers = handlers
	self.handlers['scopelist.xsl'] = scopelist_xsl

    def __call__(self, environ, start_response):
	environ.setdefault('wsgi.file_wrapper', FileWrapper)

	root = shift_path_info(environ)

	# clean up remaining path components
	path = environ['PATH_INFO']
	path = posixpath.normpath(path)
	comp = path.split('/')
	comp = filter(lambda x: x and x not in ('.','..'), comp)
	environ['PATH_INFO'] = '/'.join(comp)

	try:
	    handler = self.handlers[root]
	    response = handler(environ, start_response)
	except KeyError, IOError:
	    headers = [("Content-Type", "text/plain")]
	    start_response("404 Object not found", headers)
	    response = ['Object not found']

	if environ['REQUEST_METHOD'] == 'HEAD':
	    return [""]
	return response

