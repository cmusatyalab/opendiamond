#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2011 Carnegie Mellon University
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

__all__ = ["guess_mime_type", "DataRetriever"]

import posixpath
import mimetypes
from wsgiref.util import FileWrapper, shift_path_info

from opendiamond.helpers import connection_ok

# the following mime type guessing is from SimpleHTTPServer.py
if not mimetypes.inited:
    mimetypes.init()
_extensions = mimetypes.types_map.copy()
_extensions.update({'': 'application/octet-stream'})


def guess_mime_type(path):
    _, ext = posixpath.splitext(path)
    if ext in _extensions:
        return _extensions[ext]
    ext = ext.lower()
    if ext in _extensions:
        return _extensions[ext]
    else:
        return _extensions['']


# return xslt stylesheet which makes browsers show the scope list as thumbnails
# guaranteed to bring chaos with any decent data set.
def scopelist_xsl(environ, start_response):  # pylint: disable=unused-argument
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
class DataRetriever(object):
    def __init__(self, handlers):
        self.handlers = handlers
        self.handlers['scopelist.xsl'] = scopelist_xsl

    def __call__(self, environ, start_response):
        if not connection_ok('dataretriever', environ['REMOTE_ADDR']):
            headers = [("Content-Type", "text/plain")]
            start_response("403 Forbidden", headers)
            return "Client not authorized"

        environ.setdefault('wsgi.file_wrapper', FileWrapper)

        root = shift_path_info(environ)

        # clean up remaining path components
        path = environ['PATH_INFO']
        comp = [p for p in path.split('/') if p not in ('.', '..')]
        environ['PATH_INFO'] = '/'.join(comp)

        try:
            handler = self.handlers[root]
            response = handler(environ, start_response)
        except (KeyError, IOError):
            headers = [("Content-Type", "text/plain")]
            start_response("404 Object not found", headers)
            response = ['Object not found']

        if environ['REQUEST_METHOD'] == 'HEAD':
            return [""]
        return response
