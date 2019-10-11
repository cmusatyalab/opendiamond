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
from builtins import str
from builtins import range
import copy
from flask import Blueprint, url_for, Response, \
    stream_with_context, jsonify
from hashlib import sha256
import json
import logging
import redis
import urllib.parse
from vdms import vdms
from werkzeug.datastructures import Headers
from xml.sax.saxutils import quoteattr

BASEURL = 'yfcc100m_vdms'
STYLE = False

_log = logging.getLogger(__name__)
scope_blueprint = Blueprint('yfcc100m_store', __name__)
yfcc100m_s3_image_prefix = 'https://multimedia-commons.s3-us-west-2.amazonaws.com/data/images/'
USE_CACHE = True
BATCH_SIZE = 10000

try:
    cache = redis.StrictRedis()
    cache.ping()
    _log.info("Using cache %s", str(cache.info()))
except redis.ConnectionError:
    cache = None

USE_CACHE = USE_CACHE and cache is not None


def get_cache_key(*args):
    return 'dataretriever:' + sha256(''.join([__file__] + list(args))).hexdigest()


@scope_blueprint.route('/scope/<vdms_host>')
@scope_blueprint.route('/scope/<vdms_host>/<condition_expr>')
@scope_blueprint.route('/scope/<vdms_host>/limit/<int:limit>')
def get_scope(vdms_host, condition_expr=None, limit=None):
    _log.info('Connecting to VDMS server {}'.format(vdms_host))
    _log.info('Condition expression: %s', condition_expr)

    db = vdms.VDMS()
    db.connect(vdms_host)

    query_template = [
        {
            "FindEntity": {
                "class": "AT:IMAGE",
                "constraints": {},
                "results": {
                    "list": ["lineNumber", "mediaHash"]
                }
            }
        }
    ]

    def generate():

        yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
        if STYLE:
            yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'

        yield '<objectlist>\n'

        count = 0

        # Query VDMS in batches to avoid huge result. Batch based on lineNumber.
        # Batching is transparent to user.
        for i in range(0, 10 ** 8, BATCH_SIZE):
            query = copy.deepcopy(query_template)
            query[0]['FindEntity']['constraints']['lineNumber'] = [">=", i, "<", i + BATCH_SIZE]

            cache_key = get_cache_key(vdms_host, json.dumps(query))
            response = None
            if USE_CACHE:
                cached_response = cache.get(cache_key)
                if cached_response:
                    _log.info("Cache hit")
                    response = cached_response

            if response is None:
                response, images = db.query(query)
                if USE_CACHE:
                    _log.info("Writing cache")
                    cache.set(cache_key, response)

            response = json.loads(response)

            try:
                for entity in response[0]['FindEntity']['entities']:

                    if condition_expr:
                        condition_str = condition_expr.replace('__lineNumber__', str(entity['lineNumber']))
                        if not eval(condition_str):
                            continue

                    suffix = "{part1}/{part2}/{media_hash}.{ext}".format(
                        part1=entity['mediaHash'][:3],
                        part2=entity['mediaHash'][3:6],
                        media_hash=entity['mediaHash'],
                        ext='jpg')

                    yield '<count adjust="1"/>\n'
                    yield _get_object_element(suffix) + '\n'
                    count += 1
                    if limit is not None and count >= limit:
                        raise StopIteration
            except KeyError:
                _log.info('No results at {}'.format(i))
                break
            except StopIteration:
                _log.info('Stop at limit {}'.format(limit))
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
    return '<object id={} src={} hyperfind.external-link={} />' \
        .format(quoteattr(url_for('.get_object_id', suffix=suffix)),
                quoteattr(urllib.parse.urljoin(yfcc100m_s3_image_prefix, suffix)),
                quoteattr(urllib.parse.urljoin(yfcc100m_s3_image_prefix, suffix)))

