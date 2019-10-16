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

from __future__ import print_function
from __future__ import absolute_import
from builtins import map
from builtins import str
import json
import logging
import os
import unittest
 
from opendiamond.client.search import DiamondSearch
from opendiamond.client.rpc import ControlConnection, BlastConnection
from opendiamond.client.util import get_default_rgb_filter, get_default_scopecookies


_log = logging.getLogger()
_log.setLevel(logging.DEBUG)
_log.addHandler(logging.StreamHandler())

TEST_HOST = 'localhost'


class TestClientRPC(unittest.TestCase):
    def test_connection_pair_nonce(self):
        control = ControlConnection()
        blast = BlastConnection()
        nonce = control.connect(TEST_HOST)
        blast.connect(TEST_HOST, nonce)


class TestClientUtil(unittest.TestCase):
    def test_get_default_rgb_filter(self):
        rgb_filter = get_default_rgb_filter()
        self.assertIsNotNone(rgb_filter)
        self.assertEqual(rgb_filter.name, 'RGB')
        self.assertEqual(str(rgb_filter), 'RGB')


class TestClientSearch(unittest.TestCase):
    def test_default_rgb_filter_default_cookies(self):
        self.assertTrue(os.path.isfile(os.path.join(os.environ['HOME'], '.diamond', 'NEWSCOPE')))
        self.assertTrue(os.path.isfile(os.path.join(os.environ['HOME'], '.diamond', 'filters', 'fil_rgb')))
        cookies = get_default_scopecookies()
        _log.info("Scope: %s", '\n'.join(map(str, cookies)))
        fil_rgb = get_default_rgb_filter()
        push_attrs = ['Device-Name', 'Display-Name', '_ObjectID', '_filter.RGB_score', '_cols.int', '_rows.int']
        filters = [fil_rgb]
        search = DiamondSearch(cookies, filters, push_attrs=push_attrs)
        search_id = search.start()
        self.assertTrue(search_id)
        _log.info("Search ID %s", search_id)
        n_results = 0
        for res in search.results:
            n_results += 1
            if n_results % 10 == 0:
                _log.debug("Got %d results. %s", n_results, str(res))

        print("")
        _log.info("The last result: %s", str(res))
        _log.info("Total results: %d", n_results)

        stats = search.get_stats()
        _log.info("Stats: %s", json.dumps(stats, sort_keys=True, indent=4))

        search.close()


if __name__ == '__main__':
    unittest.main()
