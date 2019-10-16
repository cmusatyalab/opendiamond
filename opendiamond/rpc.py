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

'''Low-level RPC protocol implementation.'''

from __future__ import with_statement
from builtins import str
from builtins import object
import logging
import multiprocessing as mp
import socket
import threading

from opendiamond.xdr import XDR, XDRStruct, XDREncodingError
from future.utils import with_metaclass

_log = logging.getLogger(__name__)

RPC_PENDING = -1


class ConnectionFailure(Exception):
    '''RPC connection died.'''


class RPCError(Exception):
    '''Base class for RPC error codes.'''
    code = None

    # pylint doesn't know about __subclasses__
    # pylint: disable=no-member
    @classmethod
    def get_class(cls, code):
        '''Return the error subclass for the specified code.'''
        for c in cls.__subclasses__():
            if c.code == code:
                return c
            try:
                return c.get_class(code)
            except KeyError:
                pass
        raise KeyError('Error code %d' % code)
    # pylint: enable=no-member


class RPCEncodingError(RPCError):
    '''Bad XDR structure.'''
    code = -2


class RPCProcedureUnavailable(RPCError):
    '''Remote procedure not available.'''
    code = -3


class RPCHeader(XDRStruct):
    '''An RPC message header.'''

    ENCODED_LENGTH = 16

    members = (
        'sequence', XDR.uint(),
        'status', XDR.int(),
        'cmd', XDR.int(),
        'datalen', XDR.uint(),
    )


class _RPCRequest(object):
    '''The header and data from an RPC request.'''

    def __init__(self, hdr, data):
        self.hdr = hdr
        self.data = data

    def make_reply_header(self, status, data):
        '''Return the header for an RPC reply.'''
        return RPCHeader(sequence=self.hdr.sequence, status=status,
                         cmd=self.hdr.cmd, datalen=len(data))


class RPCConnection(object):
    '''An RPC connection.'''

    def __init__(self, sock):
        self._sock = sock   # XXX may not be pickled
        self._lock = mp.Lock()

    def _receive(self):
        '''self._lock must be held.'''
        def read_bytes(count):
            bufs = []
            try:
                while count > 0:
                    new = self._sock.recv(count)
                    if not new:
                        self._sock.close()
                        raise ConnectionFailure('Short read')
                    count -= len(new)
                    bufs.append(new)
            except socket.error as e:
                self._sock.close()
                raise ConnectionFailure(str(e))
            return b''.join(bufs)

        while True:
            hdr = RPCHeader.decode(read_bytes(RPCHeader.ENCODED_LENGTH))
            data = read_bytes(hdr.datalen)
            # We only handle request traffic; ignore reply messages
            if hdr.status == RPC_PENDING:
                return _RPCRequest(hdr, data)

    def _reply(self, request, status=0, body=b''):
        '''self._lock must be held.'''
        assert status == 0 or not body
        hdr = request.make_reply_header(status, body).encode()
        try:
            self._sock.sendall(hdr + body)
        except socket.error as e:
            self._sock.close()
            raise ConnectionFailure(str(e))

    def dispatch(self, handlers):
        '''Receive an RPC request, call a handler in handlers to process it,
        and transmit the reply.'''
        with self._lock:
            req = self._receive()
            try:
                # Look up handler and decode request
                handler_name = u'Command %d' % req.hdr.cmd
                try:
                    handler = handlers.get_handler(req.hdr.cmd)
                    handler_name = (handler.__self__.__class__.__name__ + '.' +
                                    handler.__name__)
                    if handler.rpc_request_class is not None:
                        req_obj = handler.rpc_request_class.decode(req.data)
                except KeyError:
                    raise RPCProcedureUnavailable()
                except (EOFError, XDREncodingError):
                    raise RPCEncodingError()

                # Call handler
                if handler.rpc_request_class is not None:
                    ret_obj = handler(req_obj)
                else:
                    ret_obj = handler()

                # Encode reply
                if ret_obj is None:
                    assert handler.rpc_reply_class is None
                    ret = b''
                else:
                    assert isinstance(ret_obj, handler.rpc_reply_class)
                    ret = ret_obj.encode()

                # Send reply
                self._reply(req, body=ret)
                if handlers.log_rpcs:
                    _log.debug('%s => success', handler_name)
            except RPCError as e:
                self._reply(req, status=e.code)
                if handlers.log_rpcs:
                    _log.debug('%s => %s', handler_name, e.__class__.__name__)


# _RPCMeta accesses a protected member of the classes it controls
# pylint: disable=protected-access
class _RPCMeta(type):
    '''Metaclass for RPCHandlers that collects the methods tagged with
    @RPCHandlers.handler into a dictionary.'''

    def __new__(mcs, name, bases, dct):
        obj = type.__new__(mcs, name, bases, dct)
        obj._cmds = dict()
        for attribute in dir(obj):
            f = getattr(obj, attribute)
            if hasattr(f, 'rpc_procedure'):
                # f is an unbound method
                obj._cmds[f.rpc_procedure] = f
        return obj
# pylint: enable=protected-access


class RPCHandlers(with_metaclass(_RPCMeta, object)):
    '''Base class of RPC handler objects.'''
    log_rpcs = False

    @staticmethod
    def handler(cmd, request_class=None, reply_class=None):
        '''Decorator declaring the function to be an RPC handler with the
        given command number and request class.'''
        def decorator(func):
            func.rpc_procedure = cmd
            func.rpc_request_class = request_class
            func.rpc_reply_class = reply_class
            return func
        return decorator

    # self._cmds is created by _RPCMeta
    # pylint: disable=no-member
    def get_handler(self, procedure):
        '''Returns the handler function for the specified procedure number
        or raises KeyError.'''
        # self._cmds contains unbound methods; return a bound one
        return self._cmds[procedure].__get__(self, self.__class__)
    # pylint: enable=no-member
