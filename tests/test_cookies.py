#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from builtins import str
from builtins import object
import base64
import binascii
from datetime import datetime, timedelta
from dateutil.tz import tzutc
from M2Crypto import EVP
import unittest
import uuid
import textwrap

from opendiamond.scope import ScopeCookie, ScopeError


# unittest uses Java-style naming conventions
# pylint: disable=invalid-name
class KeyPair(object):
    def __init__(self, key, cert):
        self.key = key
        self.cert = cert


KeyPair.valid = (KeyPair('''-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQDdMM19TmEBB/lwA5lym3nHwJ+xTL5pPg7F9JXzOs17TsaRI1Ot
8+r+PLGp51DPcfiTUrw6SaSbxdyVSNEavsHqh5WY4KW8SJp01rCke87utYgVIqPO
8ohikTHsOUzZ2FlRe2oHJ58kxEbmkr1/LQHM6CLd3Nq5rXW5AWVS/6sUzQIDAQAB
AoGAAfWPNGdv04cDDhtyDgJTi1Ho+DSSUTNUBSvb8ioYrNlvR9TUxmstLzBRcIxU
emnrhj6K3UdOtnSDtizNcVu0Mv/mJWiI8zdQ95mIQuGWJ65t27CAFttUEj2BwWwh
SjMK+6LIZyANoBU5Z1A7Tc/2OusnLhxE0jscWX2W9qivmr8CQQDqdvClupAN5a3i
eP0p5XVMsRmSJEaXXci2c2wMsbDWzFTiIHXLO15DzV7/A1NTA0EbNyBMd8Sca0OM
5W6Trty/AkEA8YG/FQ4fGjY/WNOVv0OKWdQQWpLHdI48jHf41oSGIXFYbg1peEhf
mEttuA7S33zx8Xry8on8X4DSiF1ie03VcwJBAKVKuJh9j7TcaNuyI/f3Vdx9NeO7
QzDO4wMNF+1TD7I+VtEbxS9DaU8vZ3ssYv+w2aNQF6sQ/ECxteuMi4n9yo0CQELe
a+wGhvIZBx0vKI1WxB3vK8AIWBWbtIQoS8wJ0aG84oFGUWeukll2uSB3akfbLppw
MoiZUDmZT7HWOYhumqUCQQCg0trsk9ymbPGckwcmfgB9CPFM8I90FPvunsSAGLL9
8hS0EClVedNWOJjTqO9SrlZcG6VVc5Kx2MAbLgUFTsck
-----END RSA PRIVATE KEY-----''', '''-----BEGIN CERTIFICATE-----
MIIBojCCAQ2gAwIBAgIETdxa4TALBgkqhkiG9w0BAQUwADAeFw0xMTA1MjUwMTI2
NThaFw0zODEwMTAwMTI3MDZaMAAwgZ0wCwYJKoZIhvcNAQEBA4GNADCBiQKBgQDd
MM19TmEBB/lwA5lym3nHwJ+xTL5pPg7F9JXzOs17TsaRI1Ot8+r+PLGp51DPcfiT
Urw6SaSbxdyVSNEavsHqh5WY4KW8SJp01rCke87utYgVIqPO8ohikTHsOUzZ2FlR
e2oHJ58kxEbmkr1/LQHM6CLd3Nq5rXW5AWVS/6sUzQIDAQABoy8wLTAMBgNVHRMB
Af8EAjAAMB0GA1UdDgQWBBTy8/ILrYkeKTF1OGIox7oB7FoARzALBgkqhkiG9w0B
AQUDgYEAVR52PU6lCIw6pEMxSiXBNiIpONlatg3uxkOoeCg0hj0cjxVFys0RGyRa
S6pNCs64IwITNT/VtGmgr+uaLXLpNjqPtOdQaPLjtsSN1qgUJDNSf6bA7TgK2gdm
bTWRjkoonfKvs+VSz591rUuGd1d/FS8cwULQ93uk7eGwIRNUhcw=
-----END CERTIFICATE-----'''), KeyPair('''-----BEGIN RSA PRIVATE KEY-----
MIICXgIBAAKBgQDEt1JQ7dG54DYYDHLeFg7jrQo/Zl/EpD6AseG8935q4L3ITyDR
kVtNFJCfX9TXuJ2FvTle9Oy9qtuXtpe1ABIEpUBPLHd98NUYj9Shr5juA/ljdk+4
r2F6H0wngz9Zo9HS8Gqx0pAXVn5jGZlGZesSDIskkW33WzRGQtcNminy7wIDAQAB
AoGATcJxhZAcoS8h8uuo1GU/yXvzWxBqtt593n1yUDI9BJ6GJpIw1OZygsuoR3eo
OQW/LsiXzxNeKbNKTnRdD61RcVYhEr9jA4gObgAv/B8DUJQVGSRzDF9Ih6yHIiyO
uwLfunNPLfaHcWBYQhPjFdihdJxOv5bBEDCjmQy1ZBnJVrECQQDImT8XXkpSC7rE
7D7LcvbsDfwPvu1WydOCdcxPCQDH3eBdQ2rII52di4uge8yrXAqfQTOsyMd01unR
UigWmhgbAkEA+wuSmQYe25SOml48C6lg5RIRydta2PL4PwzgpuR4WMYF8+ujPdXM
KMF8xpjsLvlio5YSb+n+xSTW/thuEeLlvQJBAKxHmPzb2R5/vmbzsraOROzU0d9A
DZwU+Bc6Tj6ur8H3l4LhrKq4k9xDhaZNzKh7AxBlMhk50rtQ/Dzuv0kyDHMCQQCr
jYf7fDxAD2+3u6fKzE+Dmmp/h5+4W6ka5QDr63r2JzRQMHmYOu7N9xL+X+geXZtz
cI1e3weTzw4AjwQAww39AkEAlTfJTrThfU5ufCIjqi08DoAVAFk7pyJ+Av7kJM2w
e5IStKIud/4XbyY9XzfLsJ9mUwYOA+b6xdD5pr4vJLpCDA==
-----END RSA PRIVATE KEY-----''', '''-----BEGIN CERTIFICATE-----
MIIBojCCAQ2gAwIBAgIETdxa9jALBgkqhkiG9w0BAQUwADAeFw0xMTA1MjUwMTI3
MTlaFw0zODEwMTAwMTI3MjFaMAAwgZ0wCwYJKoZIhvcNAQEBA4GNADCBiQKBgQDE
t1JQ7dG54DYYDHLeFg7jrQo/Zl/EpD6AseG8935q4L3ITyDRkVtNFJCfX9TXuJ2F
vTle9Oy9qtuXtpe1ABIEpUBPLHd98NUYj9Shr5juA/ljdk+4r2F6H0wngz9Zo9HS
8Gqx0pAXVn5jGZlGZesSDIskkW33WzRGQtcNminy7wIDAQABoy8wLTAMBgNVHRMB
Af8EAjAAMB0GA1UdDgQWBBTJtnPW4VRb50WBiU4NMdNfxBnigDALBgkqhkiG9w0B
AQUDgYEAQcTWROP9N2S6KJMSppQY/Rq5A12xeRx+gono4ENxYIMSiBu7Vu61sy3H
i50uG9R6mGmKYioAV+DQeFPB4lulgbbQtVamLWN/lDZNjJVbEyrJfr00NMJA6vfu
FJj5CeodcrHc5mcsI/4hcgfdlLlNhfWWWLdggMtUdfyYPQvhmQA=
-----END CERTIFICATE-----'''))
KeyPair.expired = KeyPair('''-----BEGIN RSA PRIVATE KEY-----
MIICWwIBAAKBgQC3N1er9/MgEtsYK1E90lO4BNXV/fbJMNNhtUPxkcBaaUUayQmy
TEuoXMSOtCnzMcI5wwbd65lcG+lnnzmkLRMb+0UVKv4UWaytjbZhgqQlaMvCFhOl
uC/D7tXkru0NX8ZGEh2wjiX9nsNCluxpnwuXr0NcOD+dhPfu3tfXBz8pHQIDAQAB
AoGAGUDkUXSJeyoGL3hnCAup1PM7qzWIctZLSIw024LwvbWFXCoKL1yDUCdLQ3Uz
VA1PfWkzlNqgxpilTQ5eIlJBMA4p+2EB88CZCVHaP+o00grphonGZHwv5v6tp9Da
SgJgTYVEGrZ5OfCS4yRgcjlxW1vwcXiIZ3tXNxKFmS1zthECQQDPeCTLFyN+njNR
E1sIEmoP9coVVCqL93TjAdGtf+UJbDMu8vA/AIw/pDTynfpxB6wQ32u9lrYSQFb3
eTbGo9ZtAkEA4hLaFfCurlQIW1Ooy8GG3o/iz3lljBi0bepDsWnJ/yfhQzT0QPMk
a7ATaJeP6ICxFOfdCeu1IYvqzhQi6x2vcQJAAOB8H1OgtcdLZjtTtiwFwL2ENiTd
7SuFlfQLA9W3jRuk97zVIR8KeLZj9uaOCW5D3upi1TFO4bLd6zva7GoC3QJAP8BT
MZaym8RkquRXmEXVs5NdwWYZZb1dvBUwy6nqZYKoelxHeL1YCuoXPwpmcYlA5oVQ
BskqRfB/4Wc6RZUUcQJAbULR4fpNBEbyhuxxJpu7RBBZTbGho74Bu9tPBDOebYC7
lM9mFmU2rPGq92DwSnWSBRSv5bXKFv9QpM6pPcStRg==
-----END RSA PRIVATE KEY-----''', '''-----BEGIN CERTIFICATE-----
MIIBojCCAQ2gAwIBAgIETdxbKTALBgkqhkiG9w0BAQUwADAeFw0xMTA1MjUwMTI4
MDlaFw0xMTA1MjYwMTI4MTBaMAAwgZ0wCwYJKoZIhvcNAQEBA4GNADCBiQKBgQC3
N1er9/MgEtsYK1E90lO4BNXV/fbJMNNhtUPxkcBaaUUayQmyTEuoXMSOtCnzMcI5
wwbd65lcG+lnnzmkLRMb+0UVKv4UWaytjbZhgqQlaMvCFhOluC/D7tXkru0NX8ZG
Eh2wjiX9nsNCluxpnwuXr0NcOD+dhPfu3tfXBz8pHQIDAQABoy8wLTAMBgNVHRMB
Af8EAjAAMB0GA1UdDgQWBBTzO1jx/RdQU+Lvxs3ZmgdrGgwvHzALBgkqhkiG9w0B
AQUDgYEAdyKOmlVyoqStjvTL5MiW7YKx3Yc76FS2ZqNgP6a5GPZAkBTE3EwP54f5
pkVdYwevLPmdxVZmLZcpPvh42lzBdXvbcifZDAPLEO3hbuPKW7ZbZc3QRJhWNs8Q
t1NDO6wIwEXCa50isAJzw080w70XXmdpam408tZWM7dpWb0qHss=
-----END CERTIFICATE-----''')


class _TestScope(unittest.TestCase):
    servers = ['a', 'b', 'c']
    scopeurls = ['z']
    expires = datetime.now(tz=tzutc()) + timedelta(days=1)
    key = KeyPair.valid[0].key
    cert = KeyPair.valid[1].cert + '\n' + KeyPair.valid[0].cert
    serverids = ['q', 'b']
    generate_exc = None
    parse_exc = None
    verify_exc = ScopeError

    def runTest(self):
        # Hack: if this is a base class rather than an actual test class,
        # return without doing anything.
        if self.__class__.__name__.startswith('_'):
            return

        if self.generate_exc:
            self.assertRaises(self.generate_exc, self.generate_cookie)
            return
        data = self.generate_cookie()

        if self.parse_exc:
            self.assertRaises(self.parse_exc, lambda: self.parse_cookie(data))
            return
        cookie = self.parse_cookie(data)

        if self.verify_exc:
            self.assertRaises(self.verify_exc,
                              lambda: self.verify_cookie(cookie))
            return
        self.verify_cookie(cookie)

    def generate_cookie(self):
        return ScopeCookie.generate(self.servers, self.scopeurls,
                                    self.expires, self.key).encode()

    def parse_cookie(self, data):
        return ScopeCookie.parse(data)

    def verify_cookie(self, cookie):
        cookie.verify(self.serverids, self.cert)


class TestCookieRoundTrip(_TestScope):
    '''Ensure that we can generate a cookie and verify it.'''
    verify_exc = None


class TestCookieBadGenerateTimezone(_TestScope):
    '''Try to generate a cookie with a naive expiration time.'''
    expires = datetime.now() + timedelta(days=1)
    generate_exc = ScopeError


class TestCookieExpired(_TestScope):
    '''Try to verify an expired cookie.'''
    expires = datetime.now(tz=tzutc()) - timedelta(seconds=5)


class TestCookieExpiredCert(_TestScope):
    '''Try to verify a cookie with an expired certificate.'''
    key = KeyPair.expired.key
    cert = KeyPair.expired.cert


class TestCookieMissingCert(_TestScope):
    '''Try to verify a cookie with a missing certificate.'''
    cert = KeyPair.valid[1].cert


class TestCookieBadServerName(_TestScope):
    '''Try to verify a cookie with a mismatched server ID.'''
    serverids = ['x']


class TestCookieEmptyScopeList(_TestScope):
    '''Verify a cookie with a zero-length scope list.'''
    scopeurls = []
    verify_exc = None


class _TestHandGeneratedCookie(_TestScope):
    boundary_start = '-----BEGIN OPENDIAMOND SCOPECOOKIE-----\n'
    boundary_end = '-----END OPENDIAMOND SCOPECOOKIE-----\n'
    serial = str(uuid.uuid4())
    version = 1
    expires = (datetime.now(tz=tzutc()) + timedelta(days=1)).isoformat()
    parse_exc = ScopeError
    verify_exc = None
    modify_sig = staticmethod(lambda s: s)
    modify_base64 = staticmethod(lambda s: s)

    def generate_cookie(self):
        '''Hand-generate a cookie.'''
        headers = dict()
        if self.version is not None:
            headers['Version'] = self.version
        if self.serial is not None:
            headers['Serial'] = self.serial
        if self.expires is not None:
            headers['Expires'] = self.expires
        if self.serverids is not None:
            headers['Servers'] = ';'.join(self.serverids)
        hdrbuf = ''.join('%s: %s\n' % (key, value)
                         for key, value in headers.items())
        data = hdrbuf + '\n' + '\n'.join(self.scopeurls) + '\n'
        key = EVP.load_key_string(self.key.encode())
        key.sign_init()
        key.sign_update(data.encode())
        sig = key.sign_final()
        body = self.modify_sig(binascii.hexlify(sig).decode()) + '\n' + data
        b64 = self.modify_base64(base64.b64encode(body.encode()).decode())
        return (self.boundary_start + textwrap.fill(b64, 64) + '\n' +
                self.boundary_end)


class TestSuccessfulHandGeneratedCookie(_TestHandGeneratedCookie):
    '''Ensure that hand-generated cookies can be verified.'''
    parse_exc = None


class TestCookieBadBoundary(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a bad end boundary.'''
    boundary_end = '-----END COOKIE-----\n'


class TestCookieBadVersion(_TestHandGeneratedCookie):
    '''Try to parse a cookie with an unrecognized version.'''
    version = 7


class TestCookieBadSerial(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a badly-formatted serial number.'''
    serial = '1-2-3-4-5'


class TestCookieBadExpiration(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a badly-formatted expiration date.'''
    expires = 'no'


class TestCookieBadVerifyTimezone(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a naive expiration date.'''
    expires = (datetime.now() + timedelta(days=1)).isoformat()


class TestCookieMissingExpiration(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a missing expiration date.'''
    expires = None


class TestCookieMissingServer(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a missing server list.'''
    serverids = None


class TestCookieMissingSerial(_TestHandGeneratedCookie):
    '''Try to parse a cookie with a missing serial number.'''
    serial = None


class TestCookieBadBase64(_TestHandGeneratedCookie):
    '''Try to parse a cookie with invalid base64 encoding.'''
    modify_base64 = staticmethod(lambda s: '*' + s[1:])


class TestCookieBadSignature(_TestHandGeneratedCookie):
    '''Try to verify a cookie with an invalid signature.'''
    modify_sig = staticmethod(lambda s: '331337' + s[6:])
    parse_exc = None
    verify_exc = ScopeError


if __name__ == '__main__':
    unittest.main()
