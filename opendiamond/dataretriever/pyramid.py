from __future__ import print_function
#
#  pyramid.py - Helper functions for accessing GigaPan
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from future import standard_library
standard_library.install_aliases()
from builtins import range
import json
import os
import sys
import urllib.request, urllib.parse, urllib.error
from math import ceil


def log_2(x):  # pylint: disable=invalid-name
    """Returns the smallest integer N such that
       2**N is not less than the given.
    """
    assert x > 0 and int(x) == x
    n = 0
    x = x - 1
    while x > 0:
        x = x >> 1
        n = n + 1
    return n


def round_up(x):  # pylint: disable=invalid-name
    """Returns the smallest integer N such that
       N is not less than the given.
    """
    return int(ceil(x))


def path_to_tile(level, column, row):
    """ Computes the path to a tile from the given pyramid coordinates.
        For example,

        >>> path_to_tile(level=5, column=5, row=3)
        'r00/r00123'
        >>> path_to_tile(5, 5, 5)
        'r00/r00303'
    """
    gc_tile = ['0', '1', '2', '3']
    fn = 'r'
    path = ''
    for n in range(level-1, -1, -1):
        bit = 1 << n
        index = 0
        if (column & bit) != 0:
            index = 1
        if (row & bit) != 0:
            index += 2
        fn += gc_tile[index]
    for i in range(0, len(fn) - 3, 3):
        path += '/' + fn[i:i+3]
    return (path + '/' + fn + ".jpg").lstrip('/')


def iter_coords(width, height, max_depth=None, tile_size=256):
    """Yields (level, column, row) coordinates for all levels of the
       pyramid for an image with the given width and height.
    """
    tiles_wide = round_up(width / float(tile_size))
    tiles_high = round_up(height / float(tile_size))
    levels_deep = log_2(max(tiles_wide, tiles_high)) + 1

    def inv(lvl):
        return levels_deep - lvl - 1

    def columns_at_level(lvl):
        return round_up(tiles_wide / float(1 << inv(lvl)))

    def rows_at_level(lvl):
        return round_up(tiles_high / float(1 << inv(lvl)))

    for lvl in range(0, levels_deep):
        if max_depth:
            if lvl > int(max_depth):
                break
        ncols = columns_at_level(lvl)
        nrows = rows_at_level(lvl)
        # print >> sys.stderr, "Checking level %d: %d columns and %d rows" % (
        #    lvl, ncols, nrows)
        for col in range(0, ncols):
            for row in range(0, nrows):
                yield lvl, col, row


def stat_gigapan(gigapan_id):
    """Uses the web API to look up width, height, etc. for a given gigapan."""
    try:
        api_url = 'http://api.gigapan.org/beta/gigapans/%d.json' % gigapan_id

        def http_get(url):
            return urllib.request.urlopen(url).read()

        api_response = json.loads(http_get(api_url))
        assert int(api_response.get('id')) == int(gigapan_id)
        return api_response
    except Exception:
        print("Couldn't parse the data returned by %s." % \
            api_url, file=sys.stderr)
        raise


def missing_tiles(root, gigapan_id):
    # Use dircache to cut down on overhead for existence checks
    def contents_of(dir):
        import dircache
        try:
            return dircache.listdir(dir)
        except OSError:
            return []

    ext = ".jpg"
    info = stat_gigapan(gigapan_id)
    width = info.get('width')
    height = info.get('height')
    missing = []
    # For each tile in the pyramid

    for lvl, col, row in iter_coords(width, height):
        # Derive the expected path to the file
        rel_path = path_to_tile(lvl, col, row)
        full_path = os.path.join(root, rel_path) + ext
        dir, fn = os.path.split(full_path)

        # We have to know whether it exists or not, but other
        # attributes will be indeterminate if it doesn't exist
        width = height = None
        if fn not in contents_of(dir):
            missing.append(fn)

    return missing
