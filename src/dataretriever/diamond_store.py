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

from dataretriever.util import ScopelistWrapper, guess_mime_type
import rfc822
import os
import re

# Read settings from $HOME/.diamond/diamond.config
def diamond_config():
    config = {}
    path = os.path.join(os.environ['HOME'], '.diamond', 'diamond_config')
    for line in open(path):
	if line[0] == '#': continue
	key, value = line.split(None, 1)
	config[key] = value.strip()
    return config

def diamond_textattr(path):
    try: # read attributes from '.text_attr' file
	for line in open(path + '.text_attr'):
	    m = re.match('^\s*"([^"]+)"\s*=\s*"([^"]*)"', line)
	    if not m: continue
	    yield m.groups()
    except IOError:
	pass

dconfig = diamond_config()
INDEXDIR = dconfig['INDEXDIR']
DATAROOT = dconfig['DATAROOT']

# we could return file URLs iff running locally and there are no text attributes
BASEURI = '/object'
#BASEURI = 'file://' + DATAROOT

def diamond_scope_app(environ, start_response):
    path = environ['PATH_INFO']
    index = 'GIDIDX' + path.replace(':','').upper()
    index = os.path.join(INDEXDIR, index)

    f = open(index, 'r')
    nentries = 0
    for line in f:
	nentries = nentries + 1
    f.close();

    f = open(index, 'r')
    start_response("200 OK", [('Content-Type', "text/xml")])
    return ScopelistWrapper(f, BASEURI, nentries)

# Get file handle and attributes for a Diamond object
def diamond_object_app(environ, start_response):
    path = os.path.join(DATAROOT, environ['PATH_INFO'])

    f = open(path, 'rb')
    stat = os.fstat(f.fileno())
    etag = str(stat.st_mtime) + "_" + str(stat.st_size)
    headers = [('Content-Type', guess_mime_type(path)),
	       ('Content-Length', str(stat.st_size)),
	       ('Last-Modified', rfc822.formatdate(stat.st_mtime)),
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

