#!/usr/bin/python
#
#  The OpenDiamond Platform for Interactive Search
#  Version 4
#
#  Copyright (c) 2009 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

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

