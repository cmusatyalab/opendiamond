#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import datetime
import os
import urlparse
from xml.sax.saxutils import quoteattr

from flask import Blueprint, url_for, Response, stream_with_context, send_file, \
    jsonify
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


@scope_blueprint.route('/scope')
@scope_blueprint.route('/scope/limit/<int:limit>')
def get_scope(limit=None):

    meta_file = os.path.join(DATAROOT, 'yfcc100m', 'yfcc100m.csv')

    def generate():

        with open(meta_file, 'r') as f:
            yield '<?xml version="1.0" encoding="UTF-8" ?>\n'

            yield '<objectlist>\n'
            
            count = 0
            for line in f:
                tokens = line.strip().split('\t')   # beware empty fields
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
                if limit is not None and count >= limit:
                    break

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
                quoteattr(urlparse.urljoin(YFCC100M_S3_IMAGE_HTTP_PREFIX, suffix)),
                quoteattr(urlparse.urljoin(YFCC100M_S3_IMAGE_HTTP_PREFIX, suffix)))

