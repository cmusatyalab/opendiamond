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

"""Low-level RPC protocol implementation."""

from builtins import next
from builtins import str
from builtins import object
import itertools
import logging
import socket

import binascii

from opendiamond import protocol
from opendiamond.rpc import (
    RPCHeader, RPC_PENDING, RPCError, RPCEncodingError, ConnectionFailure)

_log = logging.getLogger(__name__)


class _RPCClientConnection(object):
    """An RPC client connection.
    This class is responsible for establishing a TCP connection,
    sending and receiving a nonce, sending RPC requests with given command and body,
    and receiving RPC replys."""

    def __init__(self):
        self._sequence = itertools.count()
        self._sock = None

    def connect(self, address, nonce=protocol.NULL_NONCE):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self._sock.setsockopt(socket.SOL_TCP, socket.TCP_NODELAY, 1)

        parts = address.split(':')
        if len(parts) >= 2:
            host, port = parts[0], int(parts[1])
        else:
            host, port = parts[0], protocol.PORT

        _log.debug("Connecting to {}:{}.".format(host, port))
        self._sock.connect((host, port))

        _log.debug("Sending nonce {}".format(binascii.hexlify(nonce)))
        self._sock.sendall(nonce)

        rnonce = self._read_bytes(self._sock, protocol.NONCE_LEN)
        _log.debug("Read nonce {}".format(binascii.hexlify(rnonce)))
        return rnonce

    def close(self):
        self._sock.close()

    def _send_message(self, sequence, status, cmd, body=b''):
        hdr = RPCHeader(sequence, status, cmd, len(body))
        self._sock.sendall(hdr.encode() + body)

    def _call(self, cmd, request=None, reply_class=None):
        if request is not None:
            body = request.encode()
        else:
            body = b''
        seq = next(self._sequence)
        self._send_message(seq, RPC_PENDING, cmd, body)
        status, data = self._get_reply()

        if status == 0 and reply_class is not None:
            reply = reply_class.decode(data)
        elif data:
            raise RPCEncodingError('Unexpected reply data')
        elif status is None:
            raise ConnectionFailure('Connection closed')
        elif status != 0:
            try:
                raise RPCError.get_class(status)()
            except KeyError:
                err = RPCError()
                err.code = status
                raise err
        else:
            reply = None

        return reply

    def _get_reply(self):
        buf = self._read_bytes(self._sock, RPCHeader.ENCODED_LENGTH)
        hdr = RPCHeader.decode(buf)
        # Always read the body, regardless of encoding errors, to
        # keep the stream in sync
        data = self._read_bytes(self._sock, hdr.datalen)
        if hdr.status == RPC_PENDING:
            _log.warning('RPC client received request message')
            self._send_message(hdr.sequence, RPCEncodingError.code,
                               hdr.cmd)

        return hdr.status, data

    # pylint: enable=broad-except
    def _read_bytes(self, sock, count):
        """Blocking read certain bytes from a socket"""
        bufs = []
        try:
            while count > 0:
                new = sock.recv(count)
                if not new:
                    sock.close()
                    raise ConnectionFailure('Short read')
                count -= len(new)
                bufs.append(new)
        except socket.error as e:
            sock.close()
            raise ConnectionFailure(str(e))
        return b''.join(bufs)


# pylint doesn't understand that this is an instance method factory
# pylint: disable=protected-access
def _stub(cmd, request_class=None, reply_class=None):
    if request_class is not None:
        request_type = request_class
    else:
        request_type = type(None)

    def call(self, request=None):
        if not isinstance(request, request_type):
            raise RPCEncodingError('Incorrect request type')
        return self._call(cmd, request, reply_class)
    return call
# pylint: enable=protected-access


class ControlConnection(_RPCClientConnection):
    setup = _stub(25, protocol.XDR_setup, protocol.XDR_blob_list)
    send_blobs = _stub(26, protocol.XDR_blob_data)
    start = _stub(28, protocol.XDR_start)
    reexecute_filters = _stub(30, protocol.XDR_reexecute,
                              protocol.XDR_attribute_list)
    request_stats = _stub(29, None, protocol.XDR_search_stats)
    session_variables_get = _stub(18, None, protocol.XDR_session_vars)
    session_variables_set = _stub(19, None, protocol.XDR_session_vars)

    # We intentionally omit the nonce argument
    # pylint: disable=arguments-differ
    def connect(self, address):
        return _RPCClientConnection.connect(self, address)


class BlastConnection(_RPCClientConnection):
    get_object = _stub(2, None, protocol.XDR_object)

    # We intentionally make the nonce mandatory
    # pylint: disable=signature-differs
    def connect(self, address, nonce):
        return _RPCClientConnection.connect(self, address, nonce=nonce)
    # pylint: enable=signature-differs
