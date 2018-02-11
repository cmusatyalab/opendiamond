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

On ubuntu requires:
pip install mysql-connector-python-rf==2.2.2
"""
import os
from flask import Blueprint, url_for, Response, \
    stream_with_context, abort
import logging
import mysql.connector
from werkzeug.datastructures import Headers

BASEURL = 'mysql/v1'
STYLE = False
LOCAL_OBJ_URI = True  # not used. if true, return local path, otherwise http.
DATAROOT = None
DB_HOST = DB_DBNAME = DB_USER = DB_PASSWORD = DB_PORT = None

_log = logging.getLogger(__name__)


def init(config):
    global DATAROOT  # pylint: disable=global-statement
    DATAROOT = config.dataroot
    global DB_HOST, DB_DBNAME, DB_USER, DB_PASSWORD, DB_PORT
    DB_HOST = config.db_host
    DB_DBNAME = config.db_dbname
    DB_USER = config.db_user
    DB_PASSWORD = config.db_password
    DB_PORT = config.db_port


scope_blueprint = Blueprint('metadb_store', __name__)


@scope_blueprint.route('/scope/<dataset>')
@scope_blueprint.route('/scope/<dataset>/keywords/<keywords>')
@scope_blueprint.route('/scope/<dataset>/modulo/<int:divisor>/<int:remainder>')
@scope_blueprint.route(
    '/scope/<dataset>/keywords/<keywords>/modulo/<int:divisor>/<int:remainder>')
def get_scope(dataset, keywords=None, divisor=None, remainder=None):
    """

    :param dataset:
    :param keywords: a string of comma-separated keywords
    :param divisor: positive int
    :param remainder: positive int. Must be smaller than remainder
    :return:
    """
    # cursor.execute() can't substitute table name
    query = "SELECT sequence_no, rel_path FROM " + dataset
    conditions = []
    substitutes = []
    if keywords:
        conditions.append("MATCH (title, keywords, description) AGAINST(%s)")
        substitutes.append(keywords)

    if divisor:
        conditions.append("sequence_no % %s < %s")
        substitutes.extend([divisor, remainder])

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
        for seq_no, rel_path in cursor:
            yield '<count adjust="1"/>\n'
            yield _get_object_element(dataset, seq_no, rel_path) + '\n'

        yield '</objectlist>\n'

    headers = Headers([('Content-Type', 'text/xml')])

    return Response(stream_with_context(generate()),
                    status="200 OK",
                    headers=headers)


@scope_blueprint.route('/id/<dataset>/<int:seq_no>')
def get_object_id(dataset, seq_no):
    query = "SELECT rel_path FROM " + \
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

    rel_path = row[0]
    headers = Headers([('Content-Type', 'text/xml')])
    return Response(_get_object_element(dataset, seq_no, rel_path),
                    "200 OK",
                    headers=headers)


def _get_object_element(dataset, seq_no, rel_path):
    return '<object id="{}" src="{}" />' \
        .format(url_for('.get_object_id',
                        dataset=dataset, seq_no=seq_no),
                'file://' + os.path.join(DATAROOT, dataset, rel_path))
