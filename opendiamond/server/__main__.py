#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from __future__ import print_function
from builtins import str
from optparse import OptionParser
import sys

from opendiamond.config import DiamondConfig
from opendiamond.protocol import PORT
from opendiamond.server.server import DiamondServer

# Create option parser
# pylint: disable=invalid-name
parser = OptionParser()
attrs = set()


def add_option(*args, **kwargs):
    opt = parser.add_option(*args, **kwargs)
    attrs.add(opt.dest)


# Configure options
# dest should reflect attr names in DiamondConfig
add_option('-d', dest='daemonize', action='store_true', default=False,
           help='Run as a daemon')
add_option('-e', metavar='SPEC',
           dest='debug_filters', action='append', default=[],
           help='filter name/signature to run under debugger')
add_option('-E', metavar='COMMAND',
           dest='debug_command', action='store', default='valgrind',
           help='debug command to use with -e (default: valgrind)')
add_option('-f', dest='path',
           help='config file')
add_option('-n', dest='oneshot', action='store_true', default=False,
           help='do not fork for a new connection')
add_option('-p', dest='diamondd_port', default=PORT, help='accept new clients on port')

def run():
    opts, args = parser.parse_args()
    if args:
        parser.error('unrecognized command-line arguments')

    # Calculate DiamondConfig arguments
    kwargs = dict([(attr, getattr(opts, attr)) for attr in attrs])
    # If we are debugging, force single-threaded filter execution
    if kwargs['debug_filters']:
        kwargs['threads'] = 1

    # Create config object and server
    try:
        config = DiamondConfig(**kwargs)
        server = DiamondServer(config)
    except Exception as e:  # pylint: disable=broad-except
        print(str(e))
        sys.exit(1)

    # Run the server
    server.run()


if __name__ == '__main__':
    run()
