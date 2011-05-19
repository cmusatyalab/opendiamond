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

from datetime import datetime
import logging
from logging.handlers import TimedRotatingFileHandler
import os
import signal
import sys

import opendiamond
from opendiamond.helpers import daemonize
from opendiamond.server.child import ChildManager
from opendiamond.server.listen import ConnListener
from opendiamond.server.rpc import RPCConnection, ConnectionFailure
from opendiamond.server.search import Search

_log = logging.getLogger(__name__)

class _Signalled(BaseException):
    def __init__(self, sig):
        self.signal = sig
        # Find the signal name
        for attr in dir(signal):
            if (attr.startswith('SIG') and not attr.startswith('SIG_') and
                    getattr(signal, attr) == sig):
                self.signame = attr
                break
        else:
            self.signame = 'unknown signal'


class DiamondServer(object):
    caught_signals = (signal.SIGINT, signal.SIGTERM)

    def __init__(self, config):
        # Daemonize before doing any other setup
        if config.daemonize:
            daemonize()

        self.config = config
        self._children = ChildManager(config.cgroupdir, not config.oneshot)
        self._listener = ConnListener(config.localhost_only)

        # Configure signals
        for sig in self.caught_signals:
            signal.signal(sig, self._handle_signal)

        # Configure logging
        baselog = logging.getLogger()
        baselog.setLevel(logging.DEBUG)
        if not config.daemonize:
            # In daemon mode, stderr goes to /dev/null, so don't bother
            # logging there.
            handler = logging.StreamHandler()
            baselog.addHandler(handler)
        self._logfile_handler = TimedRotatingFileHandler(
                                os.path.join(config.logdir, 'diamondd.log'),
                                when='midnight', backupCount=14)
        baselog.addHandler(self._logfile_handler)

    def run(self):
        # Log startup of parent
        try:
            _log.info('Starting supervisor %s, pid %d',
                                        opendiamond.__version__, os.getpid())
            _log.info('Server IDs: %s', ', '.join(self.config.serverids))
            if self.config.cache_server:
                _log.info('Cache: %s:%d' % self.config.cache_server)
            while True:
                # Accept a new connection pair
                control, data = self._listener.accept()
                # Fork a child for this connection pair.  In the child, this
                # does not return.
                self._children.start(self._child, control, data)
                # Close the connection pair in the parent
                control.close()
                data.close()
        except _Signalled, s:
            _log.info('Supervisor exiting on %s', s.signame)
            # Stop listening for incoming connections
            self._listener.shutdown()
            # Kill our children and clean up after them
            self._children.kill_all()
            # Shut down logging
            logging.shutdown()
            # Ensure our exit status reflects that we died on the signal
            signal.signal(s.signal, signal.SIG_DFL)
            os.kill(os.getpid(), s.signal)
        except Exception:
            _log.exception('Supervisor exception')
            # Don't attempt to shut down cleanly; just flush logging buffers
            logging.shutdown()
            sys.exit(1)

    def _child(self, control, data):
        '''Main function for child process.'''
        # Close supervisor log, open child log
        baselog = logging.getLogger()
        baselog.removeHandler(self._logfile_handler)
        del self._logfile_handler
        now = datetime.now().strftime('%Y-%m-%d-%H:%M:%S')
        logname = 'search-%s-%d.log' % (now, os.getpid())
        logpath = os.path.join(self.config.logdir, logname)
        baselog.addHandler(logging.FileHandler(logpath))

        # Okay, now we have logging
        try:
            # Close listening socket and half-open connections
            self._listener.shutdown()
            # Log startup of child
            _log.info('Starting search %s, pid %d', opendiamond.__version__,
                                    os.getpid())
            _log.info('Peer: %s', control.getpeername()[0])
            _log.info('Worker threads: %d', self.config.threads)
            # Set up connection wrappers and search object
            control = RPCConnection(control)
            search = Search(self.config, RPCConnection(data))
            # Dispatch RPCs on the control connection until we die
            while True:
                control.dispatch(search)
        except ConnectionFailure, e:
            # Client closed connection
            _log.info('Search exiting: %s', str(e))
        except _Signalled, s:
            # Worker threads raise SIGUSR1 when they've encountered a
            # fatal error
            if s.signal != signal.SIGUSR1:
                _log.info('Search exiting on %s', s.signame)
        except Exception:
            _log.exception('Control thread exception')
        finally:
            logging.shutdown()

    def _handle_signal(self, sig, frame):
        '''Signal handler in the supervisor.'''
        raise _Signalled(sig)
