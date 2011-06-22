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

from dataretriever.util import guess_mime_type
from wsgiref.util import shift_path_info
from urllib import quote
from urllib2 import urlopen
from pyramid import *
import rfc822
import os
import re

__all__ = ['scope_app', 'object_app']

gigapan_info = 0

def scope_app(environ, start_response):
    root = shift_path_info(environ)
    if root == 'obj':
	return object_app(environ, start_response)
    
    gigapan_id = root.strip()

    start_response("200 OK", [('Content-Type', "text/xml")])
    return ExpandUrls(int(gigapan_id))
    

def ExpandUrls (id):
    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    yield '<objectlist>\n'

    global gigapan_info
    gigapan_info = stat_gigapan(id)
    height = gigapan_info.get('height')
    width = gigapan_info.get('width')
    levels = gigapan_info.get('levels')
    yield 'Height: %d, Width: %d, Levels: %d' % (height, width, levels)

    # still calculating stupidly
    iter = iter_coords(width, height)
    nPhotos = 0
    try:
        while 1:
            coord = iter.next()
            nPhotos += 1
    except (StopIteration):
        yield '<count adjust="%d"/>\n' % nPhotos

    iter = iter_coords(width, height)
    
    try:
        while 1:
            coord = iter.next()
            yield '<object src="obj/%s/%s/%s/%s"/>\n' % (id, coord[0], coord[1], coord[2])
    except (StopIteration):
        pass
    
    yield '</objectlist>'
        
        

def object_app(environ, start_response):
    components = environ['PATH_INFO'][1:].split('/')
    id = components[0]
    lvl = int(components[1])
    col = int(components[2])
    row = int(components[3])
    url_components = ["http://share.gigapan.org/gigapans0/", str(id), '/tiles/',path_to_tile(lvl, col, row)]
    url = ''.join(url_components)

    obj = urlopen(url)
    global gigapan_info

    headers = [ # copy some headers for caching purposes
        ('Content-Length',		obj.headers['Content-Length']),
        ('Content-Type',		obj.headers['Content-Type']),
        ('Last-Modified',		obj.headers['Last-Modified']),
        ('x-attr-gigapan_id',           id),
        ('x-attr-gigapan_height',       gigapan_info.get('height')),
        ('x-attr-gigapan_width',        gigapan_info.get('width')),
        ('x-attr-gigapan_levels',       gigapan_info.get('levels')),
        ('x-attr-tile_level',        lvl),
        ('x-attr-tile_col',          col),
        ('x-attr-tile_row',          row),
        ]
    print(headers)
    start_response("200 OK", headers)
    return environ['wsgi.file_wrapper'](obj, 65536)
