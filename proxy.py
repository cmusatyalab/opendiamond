#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2017-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import docker
import docker.errors
import socket
import binascii
from functools import wraps
import logging
import multiprocessing as mp
import os
import signal
import subprocess
import threading
from collections import deque
import sys

from opendiamond import protocol
from opendiamond.helpers import signalname
from opendiamond.blobcache import ExecutableBlobCache
from opendiamond.protocol import (
    DiamondRPCFailure, DiamondRPCFCacheMiss, DiamondRPCCookieExpired,
    DiamondRPCSchemeNotSupported)
from opendiamond.server.child import ChildManager
from opendiamond.scope import ScopeCookie, ScopeError, ScopeCookieExpired
from opendiamond.server.listen import ConnListener
from opendiamond.client.util import get_default_scopecookies
from opendiamond.proxy.search import ProxySearch
from opendiamond.protocol import (
    XDR_setup, XDR_filter_config, XDR_blob_data, XDR_start, XDR_reexecute,
    DiamondRPCFCacheMiss)
from opendiamond.rpc import RPCConnection, ConnectionFailure

HYPERFIND_PORT = 5904
DIAMOND_PORT = 5874

MOBILENET_IMAGE = ("registry.cmusatyalab.org/zf/diamond-public-registry/filter/"
                   "service@sha256:d1fa13469f63e15a205f10d4bba8d5a21696cf7a4c3c560f7b405f7b2ff0459c")


logging.info('Starting logger for...')
_log = logging.getLogger(__name__)
_log.setLevel(logging.DEBUG)


class _Signalled(BaseException):
    '''Exception indicating that a signal has been received.'''

    def __init__(self, sig):
        BaseException.__init__(self)
        self.signal = sig
        self.signame = signalname(sig)

class DiamondProxy(object):
    caught_signals = (signal.SIGINT, signal.SIGTERM, signal.SIGUSR1)

    def __init__(self):
        """ Setup the proxy server to listen to the client """
        self._listener = ConnListener(HYPERFIND_PORT)
        self._children = ChildManager(None, True)
        self._ignore_signals = False
        self.docker_address = None

        self.docker_name = 'dnn_docker'

        try:
            client = docker.from_env()
            client.images.pull(MOBILENET_IMAGE)
        except docker.errors.APIError as e:
            sys.exit('Unable to pull Docker image %s' % e)

        cmd = ['nvidia-docker', 'run', '--detach', '--name', self.docker_name,
               '--rm', MOBILENET_IMAGE, 'mobilenet']
        try:
            subprocess.check_call(cmd)
        except (subprocess.CalledProcessError, OSError):
            try:
                # In case error happens after the container is created
                # (e.g., unable to exec command inside)
                # Remove it at best effort
                client.containers.get(name).remove(force=True)
            except:  # pylint: disable=bare-except
                pass
            sys.exit('nvidia-docker unable to start: %s' % (cmd))
        else:
            # Retrieve and bind the container object
            while True:
                try:
                    container = client.containers.get(self.docker_name)
                    break
                except docker.errors.NotFound:
                    pass

            while not container.attrs['NetworkSettings']['IPAddress']:
                container.reload()

            self.docker_address = container.attrs['NetworkSettings']['IPAddress']

        # Configure signals
        for sig in self.caught_signals:
            signal.signal(sig, self._handle_signal)

    def run(self):
        try:
            while True:
                # Accept a new connection pair
                control, data = self._listener.accept()
                # Fork a child
                self._children.start(self._handle, control, data)
                control.close()
                data.close()
        except _Signalled as s:
            _log.info('Supervisor exiting on %s', s.signame)
            # Stop listening for incoming connections
            self._listener.shutdown()
            # Kill our children and clean up after them
            self._children.kill_all()
            #Remove docker container
            _log.info('Removing Docker container')
            try:
                client = docker.from_env()
                container = client.containers.get(self.docker_name)
            except docker.errors.NotFound:
                pass
            else:
                container.stop()
            # Ensure our exit status reflects that we died on the signal
            signal.signal(s.signal, signal.SIG_DFL)
            os.kill(os.getpid(), s.signal)
            sys.exit(0)
        except Exception:
            _log.exception('Supervisor exception')
            # Don't attempt to shut down cleanly; just flush logging buffers
            logging.shutdown()
            sys.exit(1)

    def _handle(self, control, data):
        search = None
        try:
            try:
                # Close listening socket and half-open connections
                self._listener.shutdown()
                # Log startup of child
                # Set up connection wrappers and search object
                control = RPCConnection(control)
                data = RPCConnection(data)
                search = ProxySearch(data, self.docker_address)
                # Dispatch RPCs on the control connection until we die
                while True:
                    control.dispatch(search)
            finally:
                # Ensure that further signals (particularly SIGUSR1 from
                # worker threads) don't interfere with the shutdown process.
                self._ignore_signals = False
                if search is not None:
                    search.shutdown()

        except ConnectionFailure:
            # Client closed connection
            _log.info('Client closed connection')
        except _Signalled as s:
            # Worker threads raise SIGUSR1 when they've encountered a
            # fatal error
            if s.signal != signal.SIGUSR1:
                _log.info('Search exiting on %s', s.signame)
        except Exception:
            _log.exception('Control thread exception')
        finally:
            if search is not None:
                search.shutdown()

    def _handle_signal(self, sig, _frame):
        '''Signal handler in the supervisor.'''
        if not self._ignore_signals:
            raise _Signalled(sig)



if __name__ == '__main__':
    proxy_object = DiamondProxy()
    proxy_object.run()

