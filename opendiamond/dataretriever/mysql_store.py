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
Presumes metadata of a data set is stored in a table in a MySQL database.
Presumes files belonging to a dataset are stored under DATAROOT/<dataset>/.
Metadata table only stores relative path to the above directory.
Provides keyword search over metadata to get list of objects.
Database config is obtained from DiamondConfig.
Presumes MySQL and the table is indexed with:
 FULLTEXT (title, keywords, description)

Requires:
pip install mysql-connector-python==8.0.6
"""
import datetime
import os
from flask import Blueprint, url_for, Response, \
    stream_with_context, abort, jsonify, send_file
import logging
import mysql.connector
from werkzeug.datastructures import Headers
from xml.sax.saxutils import quoteattr

BASEURL = 'mysql/v1'
STYLE = False
LOCAL_OBJ_URI = False  # if true, return local path, otherwise http.
DATAROOT = None
DB_HOST = DB_DBNAME = DB_USER = DB_PASSWORD = DB_PORT = None

_log = logging.getLogger(__name__)

yfcc100m_s3_image_prefix = 'https://multimedia-commons.s3-us-west-2.amazonaws.com/data/images/'


def init(config):
    global DATAROOT  # pylint: disable=global-statement
    DATAROOT = config.dataroot
    global DB_HOST, DB_DBNAME, DB_USER, DB_PASSWORD, DB_PORT
    DB_HOST = config.db_host
    DB_DBNAME = config.db_dbname
    DB_USER = config.db_user
    DB_PASSWORD = config.db_password
    DB_PORT = config.db_port


scope_blueprint = Blueprint('mysql_store', __name__)


@scope_blueprint.route('/scope/<dataset>')
@scope_blueprint.route('/scope/<dataset>/keywords/<keywords>')
@scope_blueprint.route('/scope/<dataset>/modulo/<int:divisor>/<expression>')
@scope_blueprint.route(
    '/scope/<dataset>/keywords/<keywords>/modulo/<int:divisor>/<expression>')
def get_scope(dataset, keywords=None, divisor=None, expression=None):
    """

    :param expression: Can be "<3", "=3", ">3", etc.
    :param dataset:
    :param keywords: a string of comma-separated keywords
    :param divisor: positive int
    :return:
    """
    # cursor.execute() can't substitute table name
    query = "SELECT sequence_no, rel_path, download_link FROM " + dataset
    conditions = []
    substitutes = []
    if keywords:
        conditions.append("MATCH (title, keywords, description) AGAINST(%s)")
        substitutes.append(keywords)

    if divisor:
        # TODO sanity check expression
        conditions.append("(sequence_no % %s) " + expression)
        substitutes.extend([divisor])

    if conditions:
        query += " WHERE " + ' AND '.join(conditions)

    _log.debug("Query used: %s, substitutes: %s", query, substitutes)

    def generate():
        cnx = mysql.connector.connect(user=DB_USER,
                                      password=DB_PASSWORD,
                                      host=DB_HOST,
                                      database=DB_DBNAME,
                                      port=DB_PORT)
        cursor = cnx.cursor()
        cursor.execute(query, substitutes)

        yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
        if STYLE:
            yield '<?xml-stylesheet type="text/xsl" href="/scopelist.xsl" ?>\n'

        yield '<objectlist>\n'
        for seq_no, rel_path, download_link in cursor:
            yield '<count adjust="1"/>\n'
            yield _get_object_element(dataset, seq_no, rel_path,
                                      download_link) + '\n'

        yield '</objectlist>\n'

    headers = Headers([('Content-Type', 'text/xml')])

    return Response(stream_with_context(generate()),
                    status="200 OK",
                    headers=headers)


@scope_blueprint.route('/id/<dataset>/<int:seq_no>')
def get_object_id(dataset, seq_no):
    query = "SELECT rel_path, download_link FROM " + \
            dataset + \
            " WHERE sequence_no = %s"

    cnx = mysql.connector.connect(user=DB_USER,
                                  password=DB_PASSWORD,
                                  host=DB_HOST,
                                  database=DB_DBNAME,
                                  port=DB_PORT)
    cursor = cnx.cursor()
    cursor.execute(query, (seq_no,))

    row = cursor.fetchone()

    if not row:
        abort(404)

    rel_path, download_link = row[0], row[1]
    headers = Headers([('Content-Type', 'text/xml')])
    return Response(_get_object_element(dataset, seq_no, rel_path, download_link),
                    "200 OK",
                    headers=headers)


@scope_blueprint.route('/meta/<dataset>/<int:seq_no>/<path:download_link>')
def get_object_meta(dataset, seq_no, download_link):
    data = {}
    data['hyperfind.external-link'] = download_link

    return jsonify(data)


@scope_blueprint.route('/obj/<dataset>/<path:rel_path>')
def get_object_src_http(dataset, rel_path):
    path = _get_obj_abosolute_path(dataset, rel_path)
    response = send_file(path,
                         cache_timeout=datetime.timedelta(
                             days=365).total_seconds(),
                         add_etags=True,
                         conditional=True)
    return response


def _get_obj_abosolute_path(dataset, rel_path):
    return os.path.join(DATAROOT, dataset, rel_path)


def _get_object_element(dataset, seq_no, rel_path, download_link):
    if LOCAL_OBJ_URI:
        src_uri = 'file://' + os.path.join(DATAROOT, dataset, rel_path)
    else:
        src_uri = url_for('.get_object_src_http', dataset=dataset, rel_path=rel_path)

    return '<object id={} src={} meta={} />' \
        .format(
        quoteattr(url_for('.get_object_id', dataset=dataset, seq_no=seq_no)),
        quoteattr(src_uri),
        quoteattr(url_for('.get_object_meta', dataset=dataset, seq_no=seq_no, download_link=download_link)))
