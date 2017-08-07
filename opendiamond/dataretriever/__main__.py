import argparse
from flask import Flask, Response
import importlib
import logging

import sys

from opendiamond.helpers import daemonize
from opendiamond.config import DiamondConfig
from opendiamond.dataretriever import SCOPELIST_XSL_BODY
import opendiamond

app = Flask(__name__)


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
                        action='store_true', default=False)

    options = parser.parse_args()

    # Load config
    kwargs = {}
    for opt in 'path', 'retriever_host', 'retriever_port':
        if getattr(options, opt) is not None:
            kwargs[opt] = getattr(options, opt)
    config = DiamondConfig(**kwargs)

    # Initialize logging
    logging.getLogger().addHandler(logging.StreamHandler())

    # Initialize app with configured store modules
    global app
    for store in config.retriever_stores:
        modname = "{}.{}_store".format(__package__, store)
        importlib.import_module(modname)
        store_module = sys.modules[modname]
        if hasattr(store_module, 'init'):
            store_module.init(config)

        # All data store modules should define a Flask blueprint
        # named `store_blueprint`
        app.register_blueprint(store_module.scope_blueprint,
                               url_prefix='/' + store_module.BASEURL)

    if options.daemonize:
        daemonize()

    # Note: this runs a development server. Not safe for production.
    # Other parameters to pass see
    # http://werkzeug.pocoo.org/docs/0.12/serving/#werkzeug.serving.run_simple
    app.run(host=config.retriever_host,
            port=config.retriever_port,
            threaded=True
            )


if __name__ == '__main__':
    run()
