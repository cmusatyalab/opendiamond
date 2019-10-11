#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2012 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Low-level RPC protocol implementation.'''

from builtins import next
from builtins import str
from builtins import object
import itertools
import logging
import socket
from tornado import gen, stack_context
from tornado.iostream import IOStream

from opendiamond import protocol
from opendiamond.rpc import (
    RPCHeader, RPC_PENDING, RPCError, RPCEncodingError, ConnectionFailure)

_log = logging.getLogger(__name__)


class _RPCClientConnection(object):
    '''An RPC client connection.'''

    def __init__(self, close_callback):
        self._stream = None
        self._sequence = itertools.count()
        self._pending = {}  # sequence -> callback
        self._pending_read = None
        self._close_callback = stack_context.wrap(close_callback)

    @gen.engine
    def connect(self, address, nonce=protocol.NULL_NONCE, callback=None):
        if self._stream is not None:
            raise RuntimeError('Attempting to reconnect existing connection')
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        sock.setsockopt(socket.SOL_TCP, socket.TCP_NODELAY, 1)
        self._stream = IOStream(sock)
        self._stream.set_close_callback(self._handle_close)
        # If the connect fails, our close callback will be called, and
        # the Wait will never return.
        self._stream.connect((address, protocol.PORT),
                             callback=(yield gen.Callback('connect')))
        self._write(nonce)
        yield gen.Wait('connect')

        rnonce = yield gen.Task(self._read, protocol.NONCE_LEN)
        # Start reply-handling coroutine
        self._reply_handler()
        if callback is not None:
            callback(rnonce)

    def close(self):
        if self._stream is not None:
            self._stream.close()
        else:
            # close() called before connect().  Synthesize the close event
            # ourselves.
            self._handle_close()

    def _handle_close(self):
        if self._pending_read is not None:
            # The pending read callback will never be called.  Call it
            # ourselves to clean up.
            self._pending_read(None)
        if self._close_callback is not None:
            cb = self._close_callback
            self._close_callback = None
            cb()

    @gen.engine
    def _read(self, count, callback=None):
        if self._pending_read is not None:
            raise RuntimeError('Double read on connection')
        self._pending_read = stack_context.wrap((yield gen.Callback('read')))
        try:
            self._stream.read_bytes(count, callback=self._pending_read)
            buf = yield gen.Wait('read')
            if buf is None:
                # _handle_close() is cleaning us up
                raise ConnectionFailure('Connection closed')
        except IOError as e:
            self.close()
            raise ConnectionFailure(str(e))
        finally:
            self._pending_read = None

        if callback is not None:
            callback(buf)

    def _write(self, data):
        try:
            self._stream.write(data)
        except IOError:
            self.close()

    def _send_message(self, sequence, status, cmd, body=''):
        hdr = RPCHeader(sequence, status, cmd, len(body))
        self._write(hdr.encode() + body)

    @gen.engine
    def _call(self, cmd, request=None, reply_class=None, callback=None):
        if request is not None:
            body = request.encode()
        else:
            body = ''
        seq = next(self._sequence)
        self._pending[seq] = stack_context.wrap((yield gen.Callback('reply')))
        self._send_message(seq, RPC_PENDING, cmd, body)

        status, data = (yield gen.Wait('reply')).args
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
        if callback is not None:
            callback(reply)

    # We intentionally have a catch-all exception handler
    # pylint: disable=broad-except
    @gen.engine
    def _reply_handler(self):
        '''Reply-handling coroutine.  Must not throw exceptions.'''
        while True:
            try:
                buf = yield gen.Task(self._read, RPCHeader.ENCODED_LENGTH)
                hdr = RPCHeader.decode(buf)
                # Always read the body, regardless of encoding errors, to
                # keep the stream in sync
                data = yield gen.Task(self._read, hdr.datalen)
            except ConnectionFailure:
                # Error out pending callbacks
                callbacks = list(self._pending.values())
                self._pending.clear()
                for callback in callbacks:
                    callback(None, '')
                return

            if hdr.status == RPC_PENDING:
                _log.warning('RPC client received request message')
                self._send_message(hdr.sequence, RPCEncodingError.code,
                                   hdr.cmd)
                continue

            try:
                callback = self._pending.pop(hdr.sequence)
            except KeyError:
                _log.warning('Received reply with no registered callback')
                # Drop on the floor
                continue

            try:
                callback(hdr.status, data)
            except Exception:
                # Don't crash the coroutine if the callback fails
                _log.exception('Reply callback raised exception')
    # pylint: enable=broad-except


# pylint doesn't understand that this is an instance method factory
# pylint: disable=protected-access
def _stub(cmd, request_class=None, reply_class=None):
    if request_class is not None:
        request_type = request_class
    else:
        request_type = type(None)

    def call(self, request=None, callback=None):
        if not isinstance(request, request_type):
            raise RPCEncodingError('Incorrect request type')
        self._call(cmd, request, reply_class, callback)
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
    def connect(self, address, callback=None):
        _RPCClientConnection.connect(self, address, callback=callback)
    # pylint: enable=arguments-differ


class BlastConnection(_RPCClientConnection):
    get_object = _stub(2, None, protocol.XDR_object)

    # We intentionally make the nonce mandatory
    # pylint: disable=signature-differs
    def connect(self, address, nonce, callback=None):
        _RPCClientConnection.connect(self, address, nonce=nonce,
                                     callback=callback)
    # pylint: enable=signature-differs
