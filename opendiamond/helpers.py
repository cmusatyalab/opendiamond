#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from __future__ import with_statement
from future import standard_library
standard_library.install_aliases()
from builtins import str
from builtins import range
from builtins import object
from ctypes import cdll, c_char_p, c_int
import logging
import mmh3
import os
import resource
import signal
import sys
from threading import Lock
from urllib.parse import urlparse

_log = logging.getLogger(__name__)


# We use os._exit() to avoid calling destructors after fork()
# pylint: disable=protected-access
def daemonize():
    # Double-fork
    if os.fork():
        os._exit(0)
    os.setsid()
    if os.fork():
        os._exit(0)
    # Close open fds
    maxfd = resource.getrlimit(resource.RLIMIT_NOFILE)[1]
    for fd in range(maxfd):
        try:
            os.close(fd)
        except OSError:
            pass
    # Open new fds
    os.open("/dev/null", os.O_RDWR)
    os.dup2(0, 1)
    os.dup2(0, 2)
# pylint: enable=protected-access


def signalname(signum):
    '''Return the name for the specified signal number.'''
    for attr in dir(signal):
        if (attr.startswith('SIG') and not attr.startswith('SIG_') and
                getattr(signal, attr) == signum):
            return attr
    return 'signal %d' % signum


def murmur(data):
    '''Return a lower-case hex string representing the MurmurHash3-x64-128
    hash of the specified data using the seed 0xbb40e64d (the first 10
    digits of pi, in hex).'''
    return mmh3.hash_bytes(data, 314159).hex()


class _TcpWrappers(object):
    '''Singleton callable that checks addresses of incoming connections
    against the TCP Wrappers access database.'''

    def __init__(self):
        self._lock = Lock()
        try:
            lib = cdll.LoadLibrary('libwrap.so.0')
            self._hosts_ctl = lib.hosts_ctl
            self._hosts_ctl.argtypes = [c_char_p] * 4
            self._hosts_ctl.restype = c_int
        except (OSError, AttributeError) as e:
            raise ImportError(str(e))

    def __call__(self, service, address):
        '''Given a service name and the remote address of a connection, return
        True if the connection should be allowed.'''
        # libwrap is not thread-safe
        with self._lock:
            ret = self._hosts_ctl(service.encode(), b'unknown', address.encode(), b'unknown')
            return ret == 1


class _DummyTcpWrappers(object):
    '''Singleton callable that pretends to be _TcpWrappers but doesn't do
    anything.'''

    def __init__(self, error):
        self._error = error

    def __call__(self, service, address):
        '''Always returns True.'''
        # TCP Wrappers failed to load.  Log an error the first time we are
        # called.
        if self._error is not None:
            _log.warning('TCP Wrappers disabled: %s', self._error)
            self._error = None
        return True


# We're creating a callable, so don't use attribute naming rules
# pylint: disable=invalid-name
try:
    connection_ok = _TcpWrappers()
except ImportError as _e:
    connection_ok = _DummyTcpWrappers(str(_e))
# pylint: enable=invalid-name


# urlparse wrapper to handle http://bugs.python.org/issue11467
# URIs starting with sha256:[0-9a-f] would fail in Python 2.7.1
def split_scheme(url):
    if sys.version_info[0:3] == (2, 7, 1) and url.startswith('sha256:'):
        return url.split(':')

    parts = urlparse(url)
    return (parts.scheme, parts.path)
