import unittest

from opendiamond.client.rpc import ControlConnection, BlastConnection

TEST_HOST = '128.2.209.111'


class ClientRPC(unittest.TestCase):
    def test_connection_pair_nonce(self):
        control = ControlConnection()
        blast = BlastConnection()
        nonce = control.connect(TEST_HOST)
        blast.connect(TEST_HOST, nonce)


if __name__ == '__main__':
    unittest.main()
