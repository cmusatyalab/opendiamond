#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import argparse

import logging
from flask import Flask, Response
import importlib

import sys

from opendiamond.helpers import daemonize
from opendiamond.config import DiamondConfig
from opendiamond.dataretriever import SCOPELIST_XSL_BODY
import opendiamond

app = Flask(__name__)

_log = logging.getLogger(__name__)

@app.route('/scopelist.xsl')
def scopelist_xsl():
    return Response(SCOPELIST_XSL_BODY, mimetype='text/xsl')


def run():
    parser = argparse.ArgumentParser(
        description="OpenDiamond DataRetriever. (OpenDiamond version {})".format(
            opendiamond.__version__))
    parser.add_argument('-f', '--config-file', dest='path')
    parser.add_argument('-l', '--listen', dest='retriever_host',
                        help='Bind with the specified listen address')
    parser.add_argument('-p', '--port', dest='retriever_port')
    parser.add_argument('-d', '--daemonize', dest='daemonize',
                        action='store_true', default=False, help='Run as a daemon')

    options = parser.parse_args()

    # Load config
    kwargs = {}
    for opt in 'path', 'retriever_host', 'retriever_port':
        if getattr(options, opt) is not None:
            kwargs[opt] = getattr(options, opt)
    config = DiamondConfig(**kwargs)

    # Initialize app with configured store modules
    global app
    for store in config.retriever_stores:
        modname = "{}.{}_store".format(__package__, store)
        try:
            importlib.import_module(modname)
        except ImportError:
            app.logger.error(
                'Unable to import {}. Did you misspell the name?'.format(
                    modname))
            raise
        store_module = sys.modules[modname]
        if hasattr(store_module, 'init'):
            store_module.init(config)

        # All data store modules should define a Flask blueprint
        # named `store_blueprint`
        try:
            app.register_blueprint(store_module.scope_blueprint,
                                   url_prefix='/' + store_module.BASEURL)
        except AttributeError:
            app.logger.error(
                'Unable to register {}. Make sure a Flask blueprint '
                '`scope_blueprint` is defined.'.format(modname))
            raise

    if options.daemonize:
        daemonize()

    logging.getLogger().setLevel(logging.DEBUG)
    logging.getLogger().addHandler(logging.StreamHandler())
    _log.info('Enabled modules: %s', ', '.join(config.retriever_stores))
    # Note: this runs a development server. Not safe for production.
    # Other parameters to pass see
    # http://werkzeug.pocoo.org/docs/0.12/serving/#werkzeug.serving.run_simple
    app.run(host=config.retriever_host,
            port=config.retriever_port,
            threaded=False,
            processes=16
            )


if __name__ == '__main__':
    run()
