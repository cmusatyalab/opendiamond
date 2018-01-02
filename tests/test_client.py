import logging

import os

from opendiamond.client.search import DiamondSearch
from opendiamond.client.rpc import ControlConnection, BlastConnection
from opendiamond.client.util import get_default_rgb_filter, get_default_scopecookies
import unittest

_log = logging.getLogger()
_log.setLevel(logging.DEBUG)
_log.addHandler(logging.StreamHandler())

TEST_HOST = 'cloudlet013.elijah.cs.cmu.edu'


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
        rgb_filter = get_default_rgb_filter()
        filters = [rgb_filter]
        search = DiamondSearch(cookies, filters)
        search_id = search.start()
        self.assertTrue(search_id)
        _log.info("Search ID %s", search_id)
        n_results = 0
        for res in search.results:
            n_results += 1
            if n_results % 10 == 0:
                print "Got %d results\r" % n_results,
        print ""
        # _log.info("The last object: %s", str(res))

        stats = search.get_stats()
        _log.info("Stats: %s", str(stats))

        search.close()


if __name__ == '__main__':
    unittest.main()
