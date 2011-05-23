#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2002-2007 Intel Corporation
#  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
#  Copyright (c) 2006-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import binascii
import errno
import logging
import os
import select
import socket
from weakref import WeakValueDictionary

# Listen parameters
BACKLOG = 16
PORT = 5872
# Nonce details
NONCE_LEN = 16
NULL_NONCE = '\x00' * 16
# Connection identifiers
CONTROL = 0
DATA = 1

_log = logging.getLogger(__name__)

class ListenError(Exception):
    pass


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

    def __init__(self, sock):
        self.sock = sock
        self.sock.setblocking(0)
        self.nonce = ''

    def read_nonce(self):
        '''Try to read the nonce.  Returns CONTROL if this is a control
        connection, DATA if a data connection, None if the caller should
        call back later.  The nonce is in self.nonce.'''
        if len(self.nonce) < NONCE_LEN:
            self.nonce += self.sock.recv(NONCE_LEN - len(self.nonce))
            if len(self.nonce) == NONCE_LEN:
                # We're done with non-blocking mode
                self.sock.setblocking(1)
        if len(self.nonce) == NONCE_LEN:
            if self.nonce == NULL_NONCE:
                self.nonce = os.urandom(NONCE_LEN)
                return CONTROL
            else:
                return DATA
        else:
            return None

    def send_nonce(self):
        self.sock.sendall(self.nonce)

    @property
    def nonce_str(self):
        return binascii.hexlify(self.nonce)

    @property
    def peer(self):
        return self.sock.getpeername()[0]


class _PendingConnPollSet(object):
    '''Convenience wrapper around a select.poll object which works with
    _PendingConns (plus one listening connection, denoted as None in the poll()
    results list) rather than file descriptors.'''

    def __init__(self, listener):
        self._listener = listener
        self._fd_to_pconn = dict()
        self._pollset = select.poll()
        self._pollset.register(listener.fileno(), select.POLLIN)

    def register(self, pconn, mask):
        '''Can also be used to update the mask for an existing pconn.'''
        fd = pconn.sock.fileno()
        self._fd_to_pconn[fd] = pconn
        self._pollset.register(fd, mask)

    def unregister(self, pconn):
        fd = pconn.sock.fileno()
        self._pollset.unregister(fd)
        del self._fd_to_pconn[fd]

    def poll(self, *args):
        while True:
            try:
                items = self._pollset.poll(*args)
            except select.error, e:
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
            if fd == self._listener.fileno():
                ret.append((None, event))
            else:
                ret.append((self._fd_to_pconn[fd], event))
        return ret

    def close(self):
        for pconn in self._fd_to_pconn.values():
            self.unregister(pconn)
        self._pollset.unregister(self._listener.fileno())


class ConnListener(object):
    def __init__(self, localhost_only=False):
        # Get a list of potential bind addresses
        if localhost_only:
            flags = 0
        else:
            flags = socket.AI_PASSIVE
        addrs = socket.getaddrinfo(None, PORT, 0, 0, 0, flags)
        # Try each one until we find one that works
        for family, type, proto, canonname, addr in addrs:
            try:
                self._listen = socket.socket(family, type, proto)
                self._listen.setsockopt(socket.SOL_SOCKET,
                                        socket.SO_REUSEADDR, 1)
                self._listen.bind(addr)
                self._listen.listen(BACKLOG)
                self._listen.setblocking(0)
            except socket.error:
                pass
            else:
                # Found one
                break
        else:
            # None of the addresses worked
            raise ListenError("Couldn't bind listening socket")

        self._poll = _PendingConnPollSet(self._listen)
        self._nonce_to_pending = WeakValueDictionary()

    def _accept(self):
        try:
            while True:
                sock, addr = self._listen.accept()
                _log.debug('New connection from %s', addr[0])
                self._poll.register(_PendingConn(sock),
                            select.POLLIN | select.POLLHUP | select.POLLERR)
        except socket.error:
            pass

    def _traffic(self, pconn, flags):
        if flags & (select.POLLHUP | select.POLLERR):
            # Connection died, clean it up.  _nonce_to_pending holds a weak
            # reference to the pconn, so this should be sufficient to GC
            # the pconn and close the fd.
            _log.warning('Connection to %s died during setup', pconn.peer)
            self._poll.unregister(pconn)
        elif flags & select.POLLIN:
            # Continue trying to read the nonce
            ret = pconn.read_nonce()
            if ret is not None:
                # Have the nonce.  We no longer need to wait on readability.
                self._poll.register(pconn, select.POLLHUP | select.POLLERR)
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
                        _log.debug('Data connection from %s, accepted ' +
                                            'nonce %s', pconn.peer,
                                            pconn.nonce_str)
                        pconn.send_nonce()
                        self._poll.unregister(control)
                        self._poll.unregister(pconn)
                        return (control.sock, pconn.sock)
                    else:
                        # No control connection for this data connection.
                        # Close it.
                        _log.warning('Data connection from %s, unknown ' +
                                            'nonce %s', pconn.peer,
                                            pconn.nonce_str)
                        self._poll.unregister(pconn)
        return None

    def accept(self):
        '''Returns a new (control, data) connection pair.'''
        while True:
            for pconn, flags in self._poll.poll():
                if pconn is None:
                    # Listening socket
                    self._accept()
                else:
                    # Traffic on a pending connection; attempt to pair it
                    ret = self._traffic(pconn, flags)
                    if ret is not None:
                        return ret

    def shutdown(self):
        '''Close listening socket and all pending connections.'''
        self._poll.close()
        self._listen.close()