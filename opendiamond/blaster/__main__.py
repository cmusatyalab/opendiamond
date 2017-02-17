#!/usr/bin/env python
#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2012 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import os
from tornado.httpserver import HTTPServer
from tornado.ioloop import IOLoop
from tornado.options import (
    define, options, parse_command_line, parse_config_file)

from opendiamond.blaster import JSONBlaster

define('config', default=os.path.expanduser('~/.diamond/blaster_config'),
       metavar='PATH', help='Config file')
define('listen', default=':8080',
       metavar='ADDRESS:PORT', help='Local address and port to listen on')
define('reverse_proxy', default=False,
       help='Run behind a reverse proxy')


def run():
    parse_command_line()
    if os.path.isfile(options.config):
        parse_config_file(options.config)

    server = HTTPServer(JSONBlaster(), xheaders=options.reverse_proxy)
    if ':' in options.listen:
        address, port = options.listen.split(':', 1)
        port = int(port)
    else:
        address, port = '', int(options.listen)
    server.listen(port, address=address)
    IOLoop.instance().start()


if __name__ == '__main__':
    run()
