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

import os

import json
from flask import Blueprint, url_for, Response, \
    stream_with_context, abort
import logging
import urlparse
from vdms import vdms
from werkzeug.datastructures import Headers
from xml.sax.saxutils import quoteattr

BASEURL = 'yfcc100m'
STYLE = False

_log = logging.getLogger(__name__)
scope_blueprint = Blueprint('yfcc100m_store', __name__)
yfcc100m_s3_image_prefix = 'https://multimedia-commons.s3-us-west-2.amazonaws.com/data/images/'
cache_file = os.path.join('/tmp', 'dataretriever-yfcc100m.cache')
USE_CACHE = False
BATCH_SIZE = 1000


@scope_blueprint.route('/scope/<path:vdms_host>')
def get_scope(vdms_host):
    _log.info('Connectint to VDMS server {}'.format(vdms_host))
    db = vdms.VDMS()
    db.connect(vdms_host)

    query_template = """
    [
        {
          "FindEntity" : {
             "class": "AT:IMAGE",
             "constraints": {
             "lineNumber": [">=", %d, "<", %d]
             },
             "results" : {
                "list" : ["mediaHash"]
             }
          }
       }
    ]
    """

    # TODO cache response?

    def generate():

        yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
        if STYLE:
            yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'

        yield '<objectlist>\n'

        for i in range(0, 10 ** 8, BATCH_SIZE):
            query = query_template % (i, i + BATCH_SIZE)

            response, images = db.query(query)
            response = json.loads(response)

            try:
                for entity in response[0]['FindEntity']['entities']:
                    suffix = "{part1}/{part2}/{media_hash}.{ext}".format(
                        part1=entity['mediaHash'][:3],
                        part2=entity['mediaHash'][3:6],
                        media_hash=entity['mediaHash'],
                        ext='jpg')

                    yield '<count adjust="1"/>\n'
                    yield _get_object_element(suffix) + '\n'
            except KeyError:
                _log.info('No results at {}'.format(i))
                break

        yield '</objectlist>\n'
        db.disconnect()

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
    return '<object id={} src={} />' \
        .format(quoteattr(url_for('.get_object_id', suffix=suffix)),
                quoteattr(urlparse.urljoin(yfcc100m_s3_image_prefix, suffix)))
