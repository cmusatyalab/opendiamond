import os
import datetime

from flask import Blueprint, url_for, Response, stream_with_context, send_file
from werkzeug.datastructures import Headers

from opendiamond.dataretriever.util import DiamondTextAttr

BASEURL = 'collection'
STYLE = False
INDEXDIR = DATAROOT = None


def init(config):
    global INDEXDIR, DATAROOT  # pylint: disable=global-statement
    INDEXDIR = config.indexdir
    DATAROOT = config.dataroot


scope_blueprint = Blueprint('diamond_store', __name__)


@scope_blueprint.route('/<gididx>')
def get_scope(gididx):
    index = 'GIDIDX' + gididx.upper()
    index = os.path.join(INDEXDIR, index)

    # Streaming response:
    # http://flask.pocoo.org/docs/0.12/patterns/streaming/
    def generate():
        num_entries = 0
        with open(index, 'r') as f:
            for _ in f.readlines():
                num_entries += 1

        with open(index, 'r') as f:
            yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
            if STYLE:
                yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'
            yield '<objectlist count="{:d}">\n'.format(num_entries)
            for path in f.readlines():
                path = path.strip()
                yield '<object src="{}"/>\n'.format(
                    'file://' + _get_obj_absolute_path(path))
            yield '</objectlist>\n'

    headers = Headers([('Content-Type', 'text/xml')])

    return Response(stream_with_context(generate()),
                    status="200 OK",
                    headers=headers)


@scope_blueprint.route('/obj/<path:obj_path>')
def get_object(obj_path):
    path = _get_obj_absolute_path(obj_path)

    headers = Headers()

    try:
        with DiamondTextAttr(path, 'r') as attr:
            for key, value in attr:
                # we probably should filter out invalid characters for HTTP headers
                key = 'x-attr-' + key
                headers.add(key, value)

    except IOError:
        pass
    # With add_etags=True, conditional=True
    # Flask should be smart enough to do 304 Not Modified
    response = send_file(path,
                         cache_timeout=datetime.timedelta(
                             days=365).total_seconds(),
                         add_etags=True,
                         conditional=True)
    response.headers.extend(headers)
    return response


def _get_obj_absolute_path(obj_path):
    return os.path.join(DATAROOT, obj_path)
