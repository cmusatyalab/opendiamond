#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2018-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from future import standard_library
standard_library.install_aliases()
from builtins import map
import datetime
from flask import abort, Blueprint, jsonify, \
    send_file, stream_with_context, request, Response, url_for
import itertools
import logging
import os
import urllib.parse
from xml.sax.saxutils import quoteattr
from werkzeug.datastructures import Headers

BASEURL = 'yfcc100m_simple'

INDEXDIR = DATAROOT = None

def init(config):
    global INDEXDIR, DATAROOT  # pylint: disable=global-statement
    INDEXDIR = config.indexdir
    DATAROOT = config.dataroot

scope_blueprint = Blueprint('yfcc100m_simple_store', __name__)

TOTAL_IMAGES = 99206564     # cut -f 25 yfcc100m_dataset | grep 0 | wc -l
YFCC100M_S3_IMAGE_HTTP_PREFIX = 'https://multimedia-commons.s3-us-west-2.amazonaws.com/data/images/'

_log = logging.getLogger(__name__)

@scope_blueprint.route('/scope')
def get_scope():
    """
    query string:
    slice=1:2000:3  start, stop, step get a slice of all data. default=::
    distribute=2of8     distribute to the 2nd server out ouf 8. (1-index). default=1of1
    """

    meta_file = os.path.join(DATAROOT, 'yfcc100m', 'yfcc100m.csv')

    try:
        # process query string args
        slice_str = request.args.get('slice', '::')
        start, stop, step = [int(x) if x else None for x in slice_str.split(':')[:3]]
        start = start or 0
        step = step or 1
        distribute_str = request.args.get('distribute', '1of1')
        n, m = list(map(int, distribute_str.split('of')[:2]))
        assert step > 0
        assert 1 <= n <= m
        # manipulate start, step to incorporate distribute params
        start += (n-1) * step
        step *= m
    except:
        abort(400)

    _log.debug("slice=%s; distribute=%s", slice_str, distribute_str)
    _log.info("Adjusted slice: start=%s stop=%s step=%s", start, stop, step)

    def generate():

        with open(meta_file, 'r') as f:
            yield '<?xml version="1.0" encoding="UTF-8" ?>\n'

            yield '<objectlist>\n'
            
            count = 0
            for line in itertools.islice(f, start, stop, step):
                tokens = line.strip().split('\t')   # beware empty fields (two consecutive \t)
                media_hash, ext, is_video = tokens[2], tokens[23], bool(int(tokens[24]))

                if is_video:
                    continue

                suffix = "{part1}/{part2}/{media_hash}.{ext}".format(
                        part1=media_hash[:3],
                        part2=media_hash[3:6],
                        media_hash=media_hash,
                        ext='jpg')  # png in the file is bogus.

                yield '<count adjust="1"/>\n'
                yield _get_object_element(suffix) + '\n'
                count += 1

            yield '</objectlist>\n'

    headers = Headers([('Content-Type', 'text/xml')])

    return Response(stream_with_context(generate()),
                    status="200 OK",
                    headers=headers)


@scope_blueprint.route('/id/<path:suffix>')
def get_object_id(suffix):
    headers = Headers([('Content-Type', 'text/xml')])
    return Response(_get_object_element(suffix),
                    "200 OK",
                    headers=headers)

def _get_object_element(suffix):
    return '<object id={} src={} hyperfind.external-link={} />' \
        .format(quoteattr(url_for('.get_object_id', suffix=suffix)),
                quoteattr(urllib.parse.urljoin(YFCC100M_S3_IMAGE_HTTP_PREFIX, suffix)),
                quoteattr(urllib.parse.urljoin(YFCC100M_S3_IMAGE_HTTP_PREFIX, suffix)))

