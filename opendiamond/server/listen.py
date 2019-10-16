#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2018 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Listening for new connections; pairing control and data connections.'''

from builtins import object
import binascii
import errno
import logging
import os
import select
import socket
from weakref import WeakValueDictionary

from opendiamond.helpers import connection_ok
from opendiamond.protocol import NONCE_LEN, NULL_NONCE

# Listen parameters
BACKLOG = 16
# Connection identifiers
CONTROL = 0
DATA = 1

_log = logging.getLogger(__name__)


class ListenError(Exception):
    '''Error opening listening socket.'''


class _ConnectionClosed(Exception):
    '''The socket has been closed.'''


class _PendingConn(object):
    '''A connection still in the matchmaking process, which is:
    1. Accept connection
    2. Read NONCE_LEN bytes, non-blockingly.
    3. If all zero, this is a control channel connection.  Generate NONCE_LEN
       bytes of nonce and send them back down the connection, then monitor
       the connection for closures so we can clean up our own state.
    4. If nonzero, this is a blast channel connection.  Look up the nonce to
       see which control channel it corresponds to.  If not found, close the
       connection.  Otherwise, we have a pairing; send the nonce back down
       the connection and start handling RPCs.'''

    def __init__(self, sock, peer):
        self.sock = sock
        self.sock.setblocking(0)
        self.peer = peer
        self.nonce = b''

    def read_nonce(self):
        '''Try to read the nonce.  Returns CONTROL if this is a control
        connection, DATA if a data connection, None if the caller should
        call back later.  The nonce is in self.nonce.'''
        if len(self.nonce) < NONCE_LEN:
            data = self.sock.recv(NONCE_LEN - len(self.nonce))
            if not data:
                raise _ConnectionClosed()
            self.nonce += data
            if len(self.nonce) == NONCE_LEN:
                # We're done with non-blocking mode
                self.sock.setblocking(1)
                if self.nonce == NULL_NONCE:
                    self.nonce = os.urandom(NONCE_LEN)
                    return CONTROL
                return DATA
            return None
        else:
            # If the socket is still readable, either we have received
            # unexpected data or the connection has died
            raise _ConnectionClosed()

    def send_nonce(self):
        '''Send the nonce back to the client.'''
        try:
            self.sock.sendall(self.nonce)
        except socket.error:
            raise _ConnectionClosed()

    @property
    def nonce_str(self):
        '''The nonce as a hex string.'''
        return binascii.hexlify(self.nonce)


class _ListeningSocket(object):
    '''A wrapper class for a listening socket which is to be added to a
    _PendingConnPollSet.'''

    def __init__(self, sock):
        self.sock = sock

    def accept(self):
        return self.sock.accept()


class _PendingConnPollSet(object):
    '''Convenience wrapper around a select.poll object which works not with
    file descriptors, but with any object with a "sock" attribute containing
    a network socket.'''

    def __init__(self):
        self._fd_to_pconn = dict()
        self._pollset = select.poll()

    def register(self, pconn, mask):
        '''Add the pconn to the set with the specified mask.  Can also be
        used to update the mask for an existing pconn.'''
        fd = pconn.sock.fileno()
        self._fd_to_pconn[fd] = pconn
        self._pollset.register(fd, mask)

    def unregister(self, pconn):
        '''Remove the pconn from the set.'''
        fd = pconn.sock.fileno()
        self._pollset.unregister(fd)
        del self._fd_to_pconn[fd]

    def poll(self):
        '''Poll for events and return a list of (pconn, eventmask) pairs.
        pconn will be None for events on the listening socket.'''
        while True:
            try:
                items = self._pollset.poll()
            except select.error as e:
                # If poll() was interrupted by a signal, retry.  If the
                # signal was supposed to be fatal, the signal handler would
                # have raised an exception.
                if e.args[0] == errno.EINTR:
                    pass
                else:
                    raise
            else:
                break

        ret = []
        for fd, event in items:
            ret.append((self._fd_to_pconn[fd], event))
        return ret

    def close(self):
        '''Unregister all connections from the pollset.'''
        for pconn in list(self._fd_to_pconn.values()):
            self.unregister(pconn)


class ConnListener(object):
    '''Manager for listening socket and connections still in the matchmaking
    process.'''

    def __init__(self, port):
        # Get a list of potential bind addresses
        addrs = socket.getaddrinfo(None, port, 0, socket.SOCK_STREAM, 0,
                                   socket.AI_PASSIVE)
        # Try to bind to each address
        socks = []
        for family, type, proto, _canonname, addr in addrs:
            try:
                sock = socket.socket(family, type, proto)
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                if family == socket.AF_INET6:
                    # Ensure an IPv6 listener doesn't also bind to IPv4,
                    # since depending on the order of getaddrinfo return
                    # values this could cause the IPv6 bind to fail
                    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
                sock.bind(addr)
                sock.listen(BACKLOG)
                sock.setblocking(0)
                socks.append(sock)
            except socket.error:
                pass
        if not socks:
            # None of the addresses worked
            raise ListenError("Couldn't bind listening socket")

        self._poll = _PendingConnPollSet()
        for sock in socks:
            self._poll.register(_ListeningSocket(sock), select.POLLIN)
        self._nonce_to_pending = WeakValueDictionary()

    def _accept(self, lsock):
        '''Accept waiting connections and add them to the pollset.'''
        try:
            while True:
                sock, addr = lsock.accept()
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                host = addr[0]
                if connection_ok('diamondd', host):
                    pconn = _PendingConn(sock, host)
                    _log.debug('New connection from %s', pconn.peer)
                    self._poll.register(pconn, select.POLLIN)
                else:
                    sock.close()
                    _log.info('Rejected connection from %s', host)
        except socket.error:
            pass

    def _traffic(self, pconn):
        '''Handle poll readiness events on the specified pconn.'''
        try:
            # Continue trying to read the nonce
            ret = pconn.read_nonce()
            if ret is not None:
                # Have the nonce.
                if ret == CONTROL:
                    _log.debug('Control connection from %s, nonce %s',
                               pconn.peer, pconn.nonce_str)
                    pconn.send_nonce()
                    self._nonce_to_pending[pconn.nonce] = pconn
                else:
                    control = self._nonce_to_pending.get(pconn.nonce, None)
                    if control is not None:
                        # We have a match!  Clean up pending state and
                        # return the connection handles.
                        _log.debug('Data connection from %s, accepted '
                                   'nonce %s', pconn.peer, pconn.nonce_str)
                        pconn.send_nonce()
                        self._poll.unregister(control)
                        self._poll.unregister(pconn)
                        return (control.sock, pconn.sock)
                    else:
                        # No control connection for this data connection.
                        # Close it.
                        _log.warning('Data connection from %s, unknown '
                                     'nonce %s', pconn.peer, pconn.nonce_str)
                        self._poll.unregister(pconn)
        except _ConnectionClosed:
            # Connection died, clean it up.  _nonce_to_pending holds a weak
            # reference to the pconn, so this should be sufficient to GC
            # the pconn and close the fd.
            _log.warning('Connection to %s died during setup', pconn.peer)
            self._poll.unregister(pconn)
        return None

    def accept(self):
        '''Returns a new (control, data) connection pair.'''
        while True:
            for pconn, _flags in self._poll.poll():
                if hasattr(pconn, 'accept'):
                    # Listening socket
                    self._accept(pconn)
                else:
                    # Traffic on a pending connection; attempt to pair it
                    ret = self._traffic(pconn)
                    if ret is not None:
                        return ret
                # pconn may now be a dead connection; allow it to be GC'd
                pconn = None

    def shutdown(self):
        '''Close listening socket and all pending connections.'''
        self._poll.close()
