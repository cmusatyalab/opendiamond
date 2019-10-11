#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

# Proxy returns a subset of a scopelist returned by the remote scopelist.
# The subset is given as the n-th server out of m.
# It effectively "distributes" a single scopelist to m servers.
# Each different "<n>of<m>" request reads the remote scopelist in full.
# But hopefully it will be efficient because we are streaming.


from future import standard_library
standard_library.install_aliases()
from flask import Blueprint, Response, stream_with_context
import logging
import urllib.request, urllib.parse, urllib.error
from urllib.request import urlopen
from urllib.parse import urljoin
from werkzeug.datastructures import Headers

try:
    from xml.etree.cElementTree import iterparse
except ImportError:
    from xml.etree.ElementTree import iterparse

BASEURL = 'proxy'
STYLE = False

scope_blueprint = Blueprint('proxy_store', __name__)

_log = logging.getLogger(__name__)


@scope_blueprint.route('/<int:index>of<int:total>/<path:dest_url>')
def get_scope(index, total, dest_url):
    dest_url = 'http://' + urllib.parse.quote(dest_url, safe='/:')

    _log.info("Proxying {} of {} from {}".format(index, total, dest_url))

    headers = Headers([('Content-Type', 'text/xml')])
    return Response(stream_with_context(_generate(dest_url, index, total)),
                    status="200 OK",
                    headers=headers)


def _generate(base_url, index, count):
    index -= 1
    seen = 0
    obj = urlopen(base_url)

    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    if STYLE:
        yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'

    yield '<objectlist>\n'

    root = None
    for ev, el in iterparse(obj, events=("start", "end")):
        if ev == 'start' and root is None:
            root = el
        if ev == 'end' and el.tag == 'object':
            if seen % count == index:
                # Forward 'id', 'src' and 'meta' attributes
                id = urljoin(base_url, el.attrib['id'])
                src = urljoin(base_url, el.attrib['src']) if 'src' in el.attrib else None
                meta = urljoin(base_url, el.attrib['meta']) if 'meta' in el.attrib else None
                yield '<count adjust="1"/>\n<object id="{}" '.format(id) \
                      + (' src="{}" '.format(src) if src else '') \
                      + (' meta="{}" '.format(meta) if meta else '') \
                      + '/>\n'
            seen += 1
            root.clear()

    yield '</objectlist>\n'
    obj.close()
