#
#  The OpenDiamond Platform for Interactive Search
#  Version 5
#
#  Copyright (c) 2009-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Scope cookie generation, parsing, and verification.'''

# Scope cookie format:
#
#	-----BEGIN OPENDIAMOND SCOPECOOKIE-----
#	<base64-encoded inner cookie>
#	-----END OPENDIAMOND SCOPECOOKIE-----
#
# Inner cookie format:
#
#	<hexadecimally-encoded signature of all following data>\n
#	Version: 1\n
#	Serial: <uuid>\n
#	Expires: <ISO-8601 timestamp>\n
#	Servers: <server1>;<server2>;<server3>
#	\n
#	<scope URLs, one per line>

import base64
import binascii
from datetime import datetime, timedelta
import dateutil.parser
from dateutil.tz import tzutc
from M2Crypto import EVP, X509
import os
import re
import textwrap
import uuid

BOUNDARY_START = '-----BEGIN OPENDIAMOND SCOPECOOKIE-----\n'
BOUNDARY_END = '-----END OPENDIAMOND SCOPECOOKIE-----\n'
COOKIE_VERSION = 1
BASE64_RE = '[A-Za-z0-9+/=\n]+'

class ScopeError(Exception):
    '''Error generating, parsing, or verifying scope cookie.'''
class ScopeCookieExpired(ScopeError):
    '''Scope cookie has expired.'''


class ScopeCookie(object):
    def __init__(self, serial, expires, servers, scopeurls, data, signature):
        '''Do not call this directly; use generate() or parse() instead.'''
        # Ensure the expiration time is tz-aware
        if expires.tzinfo is None or expires.tzinfo.utcoffset(expires) is None:
            raise ScopeError('Expiration time does not include time zone')
        self.serial = serial		# A UUID object
        self.expires = expires		# A datetime object
        self.servers = servers		# A list
        self.scopeurls = scopeurls	# The list of scope URLs
        self.data = data		# All of the above, as a string
        self.signature = signature	# Binary signature of the data

    def __str__(self):
        '''Return the decoded scope cookie.'''
        return binascii.hexlify(self.signature) + '\n' + self.data

    def __repr__(self):
        return ('<ScopeCookie %s, servers %s, expiration %s>' %
                (str(self.serial), str(self.servers), str(self.expires)))

    def __iter__(self):
        '''Return an iterator over the scope URLs.'''
        return iter(self.scopeurls)

    def encode(self):
        '''Return the encoded scope cookie.'''
        return (BOUNDARY_START +
                textwrap.fill(base64.b64encode(str(self)), 64) + '\n' +
                BOUNDARY_END)

    def verify(self, servernames, certdata):
        '''Verify the cookie against the specified server ID list and
        certificate data.'''
        now = datetime.now(tzutc())
        # Check cookie expiration
        if self.expires < now:
            raise ScopeCookieExpired('Cookie expired on %s' % self.expires)
        # Check that one of our server names matches a name in the cookie
        if len(set(servernames) & set(self.servers)) == 0:
            raise ScopeError('Cookie does not contain matching server name')
        # Split certdata into individual certificates
        begin = '-----BEGIN CERTIFICATE-----\n'
        end = '-----END CERTIFICATE-----' # no trailing newline
        certdata = re.findall(begin + BASE64_RE + end, certdata)
        # Load certificates
        certs = [X509.load_cert_string(cd) for cd in certdata]
        # Find a certificate that verifies the signature
        for cert in certs:
            # Rudimentary certificate checks
            if cert.verify() != 1:
                continue
            if cert.get_not_before().get_datetime() > now:
                continue
            if cert.get_not_after().get_datetime() < now:
                continue
            # Check the signature
            key = cert.get_pubkey()
            key.verify_init()
            key.verify_update(self.data)
            if key.verify_final(self.signature) == 1:
                # We have a match
                return
        raise ScopeError("Couldn't validate scope cookie signature")

    @classmethod
    def generate(cls, servers, scopeurls, expires, keydata):
        '''Generate and return a new ScopeCookie.  servers and scopeurls
        are lists.  expires is a timezone-aware datetime.  keydata is a
        PEM-encoded private key.'''
        # Generate scope data
        serial = str(uuid.uuid4())
        headers = (('Version', COOKIE_VERSION),
                   ('Serial', serial),
                   ('Expires', expires.isoformat()),
                   ('Servers', ';'.join(servers)))
        hdrbuf = reduce(lambda buf, hdr: buf + '%s: %s\n' % (hdr[0], hdr[1]),
                        headers, '')
        data = hdrbuf + '\n' + '\n'.join(scopeurls) + '\n'
        # Load the signing key
        key = EVP.load_key_string(keydata)
        # Sign the data
        key.sign_init()
        key.sign_update(data)
        sig = key.sign_final()
        # Return the scope cookie
        return cls(serial, expires, servers, scopeurls, data, sig)

    @classmethod
    def parse(cls, data):
        '''Parse the scope cookie data and return a ScopeCookie.'''
        # Check for boundary markers and remove them
        match = re.match(BOUNDARY_START + '(' + BASE64_RE + ')' +
                        BOUNDARY_END, data)
        if match is None:
            raise ScopeError('Invalid boundary markers')
        data = match.group(1)
        # Base64-decode contents
        try:
            data = base64.b64decode(data)
        except TypeError:
            raise ScopeError('Invalid Base64 data')
        # Split signature, header, and body
        try:
            signature, data = data.split('\n', 1)
            header, body = data.split('\n\n', 1)
        except ValueError:
            raise ScopeError('Malformed cookie')
        # Decode signature
        try:
            signature = binascii.unhexlify(signature)
        except TypeError:
            raise ScopeError('Malformed signature')
        # Parse headers
        for line in header.splitlines():
            k, v = line.split(':', 1)
            v = v.strip()
            if k == 'Version':
                try:
                    if int(v) != COOKIE_VERSION:
                        raise ScopeError('Unknown cookie version %s' % v)
                except ValueError:
                    raise ScopeError('Invalid cookie version string')
            elif k == 'Serial':
                try:
                    serial = uuid.UUID(v)
                except ValueError:
                    raise ScopeError('Invalid serial format')
            # Ignore KeyId; we have no way to generate one for new cookies
            elif k == 'Expires':
                try:
                    # Parsing ISO8601 is hard; we can't just use strptime.
                    # If the value does not specify a timezone, the
                    # class constructor will error out.
                    expires = dateutil.parser.parse(v)
                except ValueError:
                    raise ScopeError('Invalid date format')
            elif k == 'Servers':
                servers = [s.strip() for s in re.split('[;,]', v)
                            if s.strip() != '']
        # Parse body
        scopeurls = [s for s in [u.strip() for u in body.split('\n')]
                    if s != '']
        # Build scope cookie object
        try:
            return cls(serial, expires, servers, scopeurls, data, signature)
        except NameError:
            raise ScopeError('Missing cookie header')


def generate_cookie(scopeurls, servers, proxies=None, keyfile=None,
                    expires=None):
    '''High-level helper function: generate a scope cookie for the given
    scope URLs and servers and return its encoded form as a string.  keyfile
    defaults to ~/.diamond/key.pem and expiration defaults to one hour.  If
    proxies is provided, divide up the scope list among the specified list
    of proxy servers, produce one scope cookie for each proxy, and return
    the concatenation of the cookies.'''

    if keyfile is None:
        keyfile = os.path.expanduser(os.path.join('~', '.diamond', 'key.pem'))
    if expires is None:
        expires = timedelta(hours=1)
    def generate(scopeurls, servers):
        return ScopeCookie.generate(servers, scopeurls,
                                    datetime.now(tzutc()) + expires,
                                    open(keyfile).read()).encode()
    if proxies is None:
        return generate(scopeurls, servers)
    else:
        cookies = []
        n = len(proxies)
        for i in range(n):
            scope = ['/proxy/%dof%d/%s:5873%s' % (i + 1, n, server, url)
                        for url in scopeurls for server in servers]
            cookies.append(generate(scope, (proxies[i],)))
        return ''.join(cookies)


def _main():
    import sys
    args = sys.argv[1:]

    try:
        data = open(args.pop(0)).read()
        cookie = ScopeCookie.parse(data)
        print str(cookie),
    except IndexError:
        print >> sys.stderr, \
                'Usage: scope.py <cookie-file> [server-name [cert-file]]'
        sys.exit(1)
    except ScopeError, e:
        print str(e)
        sys.exit(1)

    try:
        server = args.pop(0)
        try:
            certfile = args.pop(0)
        except IndexError:
            certfile = os.path.expanduser(os.path.join('~', '.diamond',
                                    'CERTS'))
        certdata = open(certfile).read()
        print
        cookie.verify([server], certdata)
        print 'Cookie verified successfully'
    except IndexError:
        pass
    except ScopeError, e:
        print str(e)
        sys.exit(1)


if __name__ == '__main__':
    _main()
