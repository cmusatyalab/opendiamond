#!/usr/bin/python

HOST = '127.0.0.1'
PORT = 5873	# default port used to listen for requests

server_version = "DataRetriever/1.0"

from dataretriever.diamond_store import diamond_scope_app, diamond_object_app
from dataretriever.util import DataRetriever

app = DataRetriever({
    'collection':	diamond_scope_app,
    'object':		diamond_object_app,
})

try:
    from paste import httpserver
    httpserver.serve(app, host=HOST, port=PORT, server_version=server_version,
		     protocol_version='HTTP/1.1', daemon_threads=True)
except ImportError:
    from wsgiref.simple_server import make_server
    httpd = make_server(HOST, PORT, app)
    sa = httpd.socket.getsockname()
    print "DataRetriever listening on", sa[0], "port", sa[1], "..."
    httpd.serve_forever()

