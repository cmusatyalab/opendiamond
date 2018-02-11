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

"""
Caveat: all sampling methods are based on reading the whole index file
into memory first.
It may not be feasible for extremely large index files.
"""

import datetime
from flask import Blueprint, url_for, Response, stream_with_context, \
    send_file, jsonify
import os
import random
from werkzeug.datastructures import Headers

from opendiamond.dataretriever.util import DiamondTextAttr

BASEURL = 'carbon/v1'
STYLE = False
# if true, return local file path as object URI, otherwise http.
LOCAL_OBJ_URI = True
INDEXDIR = DATAROOT = None


def init(config):
    global INDEXDIR, DATAROOT  # pylint: disable=global-statement
    INDEXDIR = config.indexdir
    DATAROOT = config.dataroot


scope_blueprint = Blueprint('carbon_v1_store', __name__)


@scope_blueprint.route('/<gididx>/<int:start>:<int:stop>:<int:step>')
@scope_blueprint.route('/<gididx>/<int:start>:<int:stop>')
@scope_blueprint.route('/<gididx>/<int:start>::<int:step>')
@scope_blueprint.route('/<gididx>/:<int:stop>:<int:step>')
@scope_blueprint.route('/<gididx>/<int:start>:')
@scope_blueprint.route('/<gididx>/:<int:stop>')
@scope_blueprint.route('/<gididx>/::<int:step>')
@scope_blueprint.route('/<gididx>')
def get_scope(gididx, start=None, stop=None, step=None):
    index = 'GIDIDX' + gididx.upper()
    index = _get_index_absolute_path(index)

    def slice_list(l):
        return l[start:stop:step]

    # Streaming response:
    # http://flask.pocoo.org/docs/0.12/patterns/streaming/
    headers = Headers([('Content-Type', 'text/xml')])
    return Response(
        stream_with_context(_generate_scope_sublist(index, slice_list)),
        status="200 OK",
        headers=headers)


@scope_blueprint.route('/<gididx>/sample/<int:k>/seed/<int:seed>')
@scope_blueprint.route('/<gididx>/sample/<int:k>')
def get_scope_sample(gididx, k, seed=42):
    index = 'GIDIDX' + gididx.upper()
    index = _get_index_absolute_path(index)

    def sample_sublist(l):
        random.seed(seed)
        return random.sample(l, k)

    headers = Headers([('Content-Type', 'text/xml')])
    return Response(
        stream_with_context(_generate_scope_sublist(index, sample_sublist)),
        status="200 OK",
        headers=headers)


@scope_blueprint.route('/id/<path:object_path>')
def get_object_id(object_path):
    headers = Headers([('Content-Type', 'text/xml')])
    return Response(_get_object_element(object_path=object_path),
                    "200 OK",
                    headers=headers)


@scope_blueprint.route('/meta/<path:object_path>')
def get_object_meta(object_path):
    path = _get_obj_absolute_path(object_path)
    attrs = dict()

    try:
        with DiamondTextAttr(path, 'r') as attributes:
            for key, value in attributes:
                attrs[key] = value
    except IOError:
        pass

    return jsonify(attrs)


@scope_blueprint.route('/obj/<path:obj_path>')
def get_object_src_http(obj_path):
    path = _get_obj_absolute_path(obj_path)

    headers = Headers()
    # With add_etags=True, conditional=True
    # Flask should be smart enough to do 304 Not Modified
    response = send_file(path,
                         cache_timeout=datetime.timedelta(
                             days=365).total_seconds(),
                         add_etags=True,
                         conditional=True)
    response.headers.extend(headers)
    return response


def _generate_scope_sublist(index_file, sublist_fn):
    with open(index_file, 'r') as f:
        lines = list(f.readlines())

    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    if STYLE:
        yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'

    sublist = sublist_fn(lines)
    yield '<objectlist count="{:d}">\n'.format(len(sublist))

    for line in sublist:
        path = line.strip()
        yield _get_object_element(object_path=path) + '\n'

    yield '</objectlist>\n'


def _get_object_element(object_path):
    return '<object id="{}" src="{}" meta="{}" />' \
        .format(url_for('.get_object_id', object_path=object_path),
                _get_object_src_uri(object_path),
                url_for('.get_object_meta', object_path=object_path))


def _get_object_src_uri(object_path):
    if LOCAL_OBJ_URI:
        return 'file://' + _get_obj_absolute_path(object_path)
    else:
        return url_for('.get_object_src_http', obj_path=object_path)


def _get_obj_absolute_path(obj_path):
    return os.path.join(DATAROOT, obj_path)


def _get_index_absolute_path(index):
    return os.path.join(INDEXDIR, index)
