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

from __future__ import with_statement
from cStringIO import StringIO
from threading import Lock
from urllib2 import urlopen
from wsgiref.util import shift_path_info

from datetime import datetime, timedelta
from PIL import Image

from .pyramid import stat_gigapan, round_up, log_2, iter_coords, path_to_tile

__all__ = ['scope_app', 'object_app']
BASEURL = 'gigapan'

TILE_SIZE = 256


class GigaPanInfoCache(object):
    def __init__(self):
        self._cache = {}
        self._lock = Lock()

    def __getitem__(self, gigapan_id):
        with self._lock:
            if gigapan_id not in self._cache:
                self._cache[gigapan_id] = stat_gigapan(gigapan_id)
            return self._cache[gigapan_id]


_gigapan_info_cache = GigaPanInfoCache()


def scope_app(environ, start_response):
    root = shift_path_info(environ)
    if root == 'obj':
        return object_app(environ, start_response)

    gigapan_id = root.strip()

    start_response("200 OK", [('Content-Type', "text/xml")])
    return expand_urls(int(gigapan_id))


def tiles_in_gigapan(width, height):
    """Modified from pyramid.iter_coords()."""
    tiles_wide = round_up(width / float(TILE_SIZE))
    tiles_high = round_up(height / float(TILE_SIZE))
    levels_deep = log_2(max(tiles_wide, tiles_high)) + 1

    def inv(lvl):
        return levels_deep - lvl - 1

    def columns_at_level(lvl):
        return round_up(tiles_wide / float(1 << inv(lvl)))

    def rows_at_level(lvl):
        return round_up(tiles_high / float(1 << inv(lvl)))

    return sum(columns_at_level(lvl) * rows_at_level(lvl)
               for lvl in range(levels_deep))


def expand_urls(gigapan_id):
    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    yield '<objectlist>\n'

    info = _gigapan_info_cache[gigapan_id]
    height = info.get('height')
    width = info.get('width')
    # levels = info.get('levels')

    yield '<count adjust="%d"/>\n' % tiles_in_gigapan(width, height)

    iter = iter_coords(width, height)

    try:
        while True:
            coord = iter.next()
            yield '<object src="obj/%s/%s/%s/%s"/>\n' % (
                gigapan_id, coord[0], coord[1], coord[2])
    except StopIteration:
        pass

    yield '</objectlist>'


def object_app(environ, start_response):
    components = environ['PATH_INFO'][1:].split('/')
    gigapan_id = int(components[0])
    lvl = int(components[1])
    col = int(components[2])
    row = int(components[3])
    url_components = ["http://share.gigapan.org/gigapans0/", str(gigapan_id),
                      '/tiles/', path_to_tile(lvl, col, row)]
    url = ''.join(url_components)

    obj = urlopen(url)
    info = _gigapan_info_cache[gigapan_id]
    levels = int(info.get('levels'))
    width = int(info.get('width'))
    height = int(info.get('height'))
    real_level = (levels - 1) - lvl
    level_width = width / 2**real_level
    level_height = height / 2**real_level
    bottom_right = ((col + 1) * TILE_SIZE, (row + 1) * TILE_SIZE)
    img_width = img_height = TILE_SIZE
    if bottom_right[0] > level_width:
        img_width = TILE_SIZE - (bottom_right[0] - level_width)
    if bottom_right[1] > level_height:
        img_height = TILE_SIZE - (bottom_right[1] - level_height)
    if img_width != TILE_SIZE or img_height != TILE_SIZE:
        input_stream = StringIO(obj.read())
        im = Image.open(input_stream)
        new_image = im.crop((0, 0, img_width, img_height))
        result = StringIO()
        new_image.save(result, "PNG")
        content_length = result.tell()
        result.seek(0)
        content_type = "image/png"
    else:
        result = obj
        content_type = obj.headers['Content-Type']
        content_length = obj.headers['Content-Length']

    time = datetime.utcnow() + timedelta(days=365)
    timestr = time.strftime('%a, %d %b %Y %H:%M:%S GMT')

    headers = [  # copy some headers for caching purposes
        ('Content-Length', str(content_length)),
        ('Content-Type', content_type),
        ('Last-Modified', obj.headers['Last-Modified']),
        ('Expires', timestr),

        ('x-attr-gigapan_id', str(gigapan_id)),
        ('x-attr-gigapan_height', str(height)),
        ('x-attr-gigapan_width', str(width)),
        ('x-attr-gigapan_levels', str(levels)),
        ('x-attr-tile_level', str(lvl)),
        ('x-attr-tile_col', str(col)),
        ('x-attr-tile_row', str(row)),
    ]
    start_response("200 OK", headers)
    return environ['wsgi.file_wrapper'](result, 65536)
