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
# Functions to access the OpenDiamond content store
#

# we could return file URLs iff running locally and there are no text attributes
#OBJECT_URI = 'file://' + DATAROOT
OBJECT_URI = 'obj'

# include a reference to xslt stylesheet (only useful for debugging)
STYLE = False

from datetime import datetime, timedelta
from dataretriever.util import guess_mime_type
from opendiamond.config import DiamondConfig
from wsgiref.util import shift_path_info
from urllib import quote
import rfc822
import os
import re

__all__ = ['scope_app', 'object_app']
baseurl = 'collection'


def diamond_textattr(path):
    try: # read attributes from '.text_attr' file
	for line in open(path + '.text_attr'):
	    m = re.match('^\s*"([^"]+)"\s*=\s*"([^"]*)"', line)
	    if not m: continue
	    yield m.groups()
    except IOError:
	pass

dconfig = DiamondConfig()
INDEXDIR = dconfig.indexdir
DATAROOT = dconfig.dataroot

def GIDIDXParser(index):
    f = open(index, 'r')
    nentries = 0
    for line in f:
	nentries = nentries + 1
    f.close();

    f = open(index, 'r')
    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    if STYLE:
	yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'
    yield '<objectlist count="%d">\n' % nentries
    for path in f:
	yield '<object src="%s/%s" />\n' % (OBJECT_URI, quote(path.strip()))
    yield '</objectlist>'
    f.close()


def scope_app(environ, start_response):
    root = shift_path_info(environ)
    if root == 'obj':
	return object_app(environ, start_response)

    index = 'GIDIDX' + root.upper()
    index = os.path.join(INDEXDIR, index)

    start_response("200 OK", [('Content-Type', "text/xml")])
    return GIDIDXParser(index)


# Get file handle and attributes for a Diamond object
def object_app(environ, start_response):
    path = os.path.join(DATAROOT, environ['PATH_INFO'][1:])

    f = open(path, 'rb')
    stat = os.fstat(f.fileno())
    expire = datetime.utcnow() + timedelta(days=365)
    expirestr = expire.strftime('%a, %d %b %Y %H:%M:%S GMT')
    etag = '"' + str(stat.st_mtime) + "_" + str(stat.st_size) + '"'
    headers = [('Content-Type', guess_mime_type(path)),
	       ('Content-Length', str(stat.st_size)),
	       ('Last-Modified', rfc822.formatdate(stat.st_mtime)),
	       ('Expires', expirestr),
	       ('ETag', etag)]

    for key, value in diamond_textattr(path):
	# we probably should filter out invalid characters for HTTP headers
	key = 'x-attr-' + key
	headers.append((key, value))

    if_modified = environ.get('HTTP_IF_MODIFIED_SINCE')
    if_none = environ.get('HTTP_IF_NONE_MATCH')
    if (if_modified and (rfc822.parsedate(if_modified) >= stat.st_mtime)) or \
       (if_none and (if_none == '*' or etag in if_none)):
	start_response("304 Not Modified", headers)
	return [""]

    start_response("200 OK", headers)
    # wrap the file object in an iterator that reads the file in 64KB blocks
    # instead of line-by-line.
    return environ['wsgi.file_wrapper'](f, 65536)

