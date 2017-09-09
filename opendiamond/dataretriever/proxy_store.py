# Proxy returns a subset of a scopelist returned by the remote scopelist.
# The subset is given as the n-th server out of m.
# It effectively "distributes" a single scopelist to m servers.
# Each different "<n>of<m>" request reads the remote scopelist in full.
# But hopefully it will be efficient because we are streaming.

from urllib2 import urlopen
from urlparse import urljoin

from flask import Blueprint, Response, stream_with_context
from werkzeug.datastructures import Headers

try:
    from xml.etree.cElementTree import iterparse
except ImportError:
    from xml.etree.ElementTree import iterparse

BASEURL = 'proxy'
STYLE = False

scope_blueprint = Blueprint('proxy_store', __name__)


@scope_blueprint.route('/<int:index>of<int:total>/<path:dest_url>')
def get_scope(index, total, dest_url):
    dest_url = 'http://' + dest_url

    headers = Headers([('Content-Type', 'text/xml')])
    return Response(stream_with_context(_generate(dest_url, index, total)),
                    status="200 OK",
                    headers=headers)


def _generate(base_url, index, count, ):
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
                src = urljoin(base_url, el.attrib['src'])
                yield '<count adjust="1"/>\n<object src="%s"/>\n' % src
            seen += 1
            root.clear()

    yield '</objectlist>\n'
    obj.close()
