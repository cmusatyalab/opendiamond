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

'''Scope cookie generation, parsing, and verification.'''
from __future__ import print_function

# Scope cookie format:
#
#       -----BEGIN OPENDIAMOND SCOPECOOKIE-----
#       <base64-encoded inner cookie>
#       -----END OPENDIAMOND SCOPECOOKIE-----
#
# Inner cookie format:
#
#       <hexadecimally-encoded signature of all following data>\n
#       Version: 1\n
#       Serial: <uuid>\n
#       Expires: <ISO-8601 timestamp>\n
#       Servers: <server1>;<server2>;<server3>
#       \n
#       <scope URLs, one per line>

from builtins import str
from builtins import range
from builtins import object
import base64
import binascii
from datetime import datetime, timedelta
import os
import re
import textwrap
import uuid

import dateutil.parser
from dateutil.tz import tzutc
from M2Crypto import EVP, X509

BOUNDARY_START = '-----BEGIN OPENDIAMOND SCOPECOOKIE-----\n'
BOUNDARY_END = '-----END OPENDIAMOND SCOPECOOKIE-----\n'
COOKIE_VERSION = 1
BASE64_RE = '[A-Za-z0-9+/=\n]+'


class ScopeError(Exception):
    '''Error generating, parsing, or verifying scope cookie.'''


class ScopeCookieExpired(ScopeError):
    '''Scope cookie has expired.'''


class ScopeCookie(object):
    def __init__(self, serial, expires, blaster, servers, scopeurls, data,
                 signature):
        '''Do not call this directly; use generate() or parse() instead.'''
        assert isinstance(data, str)
        assert isinstance(signature, bytes)
        # Ensure the expiration time is tz-aware
        if expires.tzinfo is None or expires.tzinfo.utcoffset(expires) is None:
            raise ScopeError('Expiration time does not include time zone')
        self.serial = serial            # A UUID object
        self.expires = expires          # A datetime object
        self.blaster = blaster          # The URL of the JSON blaster or None
        self.servers = servers          # A list
        self.scopeurls = scopeurls      # The list of scope URLs
        self.data = data                # All of the above, as a string
        self.signature = signature      # Binary signature of the data (bytes)

    def __str__(self):
        '''Return the decoded scope cookie.'''
        return binascii.hexlify(self.signature).decode() + '\n' + self.data

    def __repr__(self):
        return ('<ScopeCookie %s, blaster %s, servers %s, urls %s, expiration %s>' %
                (self.serial, self.blaster, self.servers, self.scopeurls, self.expires))

    def __iter__(self):
        '''Return an iterator over the scope URLs.'''
        return iter(self.scopeurls)

    def encode(self):
        '''Return the encoded scope cookie as a string.'''
        return (BOUNDARY_START +
                textwrap.fill(base64.b64encode(str(self).encode()).decode(), 64) + '\n' +
                BOUNDARY_END)

    def verify(self, servernames, certdata):
        '''Verify the cookie against the specified server ID list and
        certificate data.'''
        assert isinstance(certdata, str)

        now = datetime.now(tzutc())
        # Check cookie expiration
        if self.expires < now:
            raise ScopeCookieExpired('Cookie expired on %s' % self.expires)
        # Check that one of our server names matches a name in the cookie
        if not set(servernames) & set(self.servers):
            raise ScopeError('Cookie does not contain matching server name')
        # Split certdata into individual certificates
        begin = '-----BEGIN CERTIFICATE-----\n'
        end = '-----END CERTIFICATE-----'        # no trailing newline
        certdata = re.findall(begin + BASE64_RE + end, certdata)
        # Load certificates
        certs = [X509.load_cert_string(cd) for cd in certdata]
        # Find a certificate that verifies the signature
        failure = "Couldn't validate scope cookie signature"
        for cert in certs:
            # Check the signature
            key = cert.get_pubkey()
            key.verify_init()
            key.verify_update(self.data.encode())
            if key.verify_final(self.signature) != 1:
                # Signature does not match certificate
                continue
            # Signature valid; now check the certificate
            if cert.verify() != 1:
                failure = 'Scope cookie signed by invalid certificate'
                continue
            if cert.get_not_before().get_datetime() > now:
                failure = 'Scope cookie signed by postdated certificate'
                continue
            if cert.get_not_after().get_datetime() < now:
                failure = 'Scope cookie signed by expired certificate'
                continue
            # We have a match
            return
        raise ScopeError(failure)

    @classmethod
    def generate(cls, servers, scopeurls, expires, keydata, blaster=None):
        '''Generate and return a new ScopeCookie.  servers and scopeurls
        are lists of strings, already Punycoded/URL-encoded as appropriate.
        expires is a timezone-aware datetime.  keydata is a PEM-encoded
        private key.  blaster is an optional string, already URL-encoded.'''
        # Unicode strings can cause signature validation errors
        if isinstance(keydata, str):
            keydata = keydata.encode()
        else:
            assert isinstance(keydata, bytes)

        servers = [str(s) for s in servers]
        scopeurls = [str(u) for u in scopeurls]
        if blaster is not None:
            blaster = str(blaster)
        # Generate scope data
        serial = str(uuid.uuid4())
        headers = [('Version', COOKIE_VERSION),
                   ('Serial', serial),
                   ('Expires', expires.isoformat()),
                   ('Servers', ';'.join(servers))]
        if blaster is not None:
            headers.append(('Blaster', blaster))
        hdrbuf = ''.join('%s: %s\n' % (k, v) for k, v in headers)
        data = hdrbuf + '\n' + '\n'.join(scopeurls) + '\n'
        # Load the signing key
        key = EVP.load_key_string(keydata)  # expects bytes, not str
        # Sign the data
        key.sign_init()
        key.sign_update(data.encode())
        sig = key.sign_final()
        # Return the scope cookie
        return cls(serial, expires, blaster, servers, scopeurls, data, sig)

    @classmethod
    def parse(cls, data):
        """Parse a (single) scope cookie string and return a ScopeCookie
        
        Arguments:
            data {str} -- A single base64-encoded cookie

        Returns:
            [ScopeCookie] -- [description]
        """
        assert isinstance(data, str)

        # Check for boundary markers and remove them
        match = re.match(BOUNDARY_START + '(' + BASE64_RE + ')' +
                         BOUNDARY_END, data)
        if match is None:
            raise ScopeError('Invalid boundary markers')
        data = match.group(1)
        # Base64-decode contents
        try:
            data = base64.b64decode(data).decode()
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
        blaster = None
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
            elif k == 'Blaster':
                blaster = v
        # Parse body
        scopeurls = [s for s in [u.strip() for u in body.split('\n')]
                     if s != '']
        # Build scope cookie object
        try:
            return cls(serial, expires, blaster, servers, scopeurls, data,
                       signature)
        except NameError:
            raise ScopeError('Missing cookie header')

    @classmethod
    def split(cls, data):
        '''Split the megacookie data into individual encoded scope cookies.'''
        return re.findall(BOUNDARY_START + BASE64_RE + BOUNDARY_END, data)


def generate_cookie(scopeurls, servers, proxies=None, keyfile=None,
                    expires=None, blaster=None):
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
                                    open(keyfile).read(),
                                    blaster=blaster).encode()

    if proxies is None:
        return generate(scopeurls, servers)

    cookies = []
    n = len(proxies)
    for i in range(n):
        scope = ['/proxy/%dof%d/%s:5873%s' % (i + 1, n, server, url)
                 for url in scopeurls for server in servers]
        cookies.append(generate(scope, (proxies[i],)))
    return ''.join(cookies)


# Don't complain if Django isn't installed on the build system
# pylint: disable=import-error
def generate_cookie_django(scopeurls, servers, proxies=None, blaster=None):
    '''A variant of generate_cookie() which pulls the more obscure fixed
    arguments from Django settings.

    The signing key file comes from settings.SCOPE_KEY_FILE, if present.

    The expiration time (in seconds) comes from
    settings.SCOPE_COOKIE_EXPIRATION, if present.'''

    from django.conf import settings
    keyfile = getattr(settings, 'SCOPE_KEY_FILE', None)
    expires = getattr(settings, 'SCOPE_COOKIE_EXPIRATION', None)
    if expires is not None:
        expires = timedelta(seconds=expires)
    return generate_cookie(scopeurls, servers, proxies=proxies,
                           keyfile=keyfile, expires=expires, blaster=blaster)
# pylint: enable=import-error


def get_cookie_map(cookies):
    '''Given a list of ScopeCookies, return a dict: server ->[ScopeCookie].'''
    map = {}
    for cookie in cookies:
        for server in cookie.servers:
            map.setdefault(server, []).append(cookie)
    return map


def get_blaster_map(cookies):
    '''Given a list of ScopeCookies, return a dict: blaster ->[ScopeCookie].'''
    map = {}
    for cookie in cookies:
        map.setdefault(cookie.blaster, []).append(cookie)
    map.pop(None, None)
    return map


def _main():
    import sys
    args = sys.argv[1:]
    # filename is mandatory
    try:
        filename = args.pop(0)
    except IndexError:
        print('Usage: scope.py <cookie-file> [server-name [cert-file]]', file=sys.stderr)
        sys.exit(1)
    # certificate validation is optional
    try:
        server = args.pop(0)
    except IndexError:
        server = None
    # certfile defaults to ~/.diamond/CERTS
    try:
        certfile = args.pop(0)
    except IndexError:
        certfile = os.path.expanduser(
            os.path.join('~', '.diamond', 'CERTS'))

    try:
        data = open(filename, 'rt').read()
        assert isinstance(data, str)
        cookies = [ScopeCookie.parse(c) for c in ScopeCookie.split(data)]
        print('\n\n'.join([str(c) for c in cookies]))

        if server is not None:
            certdata = open(certfile, 'rt').read()
            for cookie in cookies:
                cookie.verify([server], certdata)
            print('Cookies verified successfully')
    except ScopeError as e:
        raise


if __name__ == '__main__':
    _main()
