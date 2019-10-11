#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''
diamondd has a long-lived supervisor process responsible for accepting
connections.  The supervisor process forks to produce a child process which
handles a particular search.  This ensures that any resource leaks within
the search logic will not accumulate in a long-running process.

The supervisor diamondd process is single-threaded, and all of its network I/O
is performed non-blockingly.  It is responsible for the following:

1.  Listening for incoming control and blast channel connections and pairing
them via a nonce communicated when the connection is first established.

2.  Establishing a temporary directory and forking a child process for every
connection pair.

3.  Cleaning up after search processes which have exited by deleting their
temporary directories and killing all of their children (filters and helper
processes).

The child is responsible for handling the search.  Initially it has only one
thread, which is responsible for handling the control connection back to the
client.  All client RPCs, including search reexecution, are handled in this
thread.  When the start() RPC is received, the client creates N worker
threads (configurable, defaulting to the number of processors on the
machine) to process objects for the search.

Several pieces of mutable state are shared between threads.  The control
thread configures a ScopeListLoader which iterates over the in-scope Diamond
objects, returning a new object to each worker thread that asks for one.
The blast channel is also shared.  There are also shared objects for logging
and for tracking of statistics and session variables.  All of these objects
have locking to ensure consistency.

Each worker thread maintains a private TCP connection to the Redis server,
which is used for result and attribute caching.  Each worker thread also
maintains one child process for each filter in the filter stack.  These
children are the actual filter code, and communicate with the worker thread
via a pair of pipes.  Because each worker thread has its own set of filter
processes, worker threads can process objects independently.

Each worker thread executes a loop:

1.  Obtain a new object from the ScopeListLoader.

2.  Retrieve result cache entries from Redis.

3.  Walk the result cache entries to determine if a drop decision can be
made.  If so, drop the object.

4.  For each filter in the filter chain, determine whether we received a
valid result cache entry for the filter.  If so, attempt to obtain attribute
cache entries from Redis.  If successful, merge the cached attributes into
the object.  Otherwise, execute the filter.  If the filter produces a drop
decision, break.

5.  Transmit new result cache entries, as well as attribute cache entries
for filters producing less than 2 MB/s of attribute values, to Redis.

6.  If accepting the object, transmit it to the client via the blast
channel.

If a filter crashes while processing an object, the object is dropped and
the filter is restarted.  If a worker thread or the control thread crashes,
the exception is logged and the entire search is terminated.
'''

from builtins import object
from datetime import datetime, timedelta
import logging
from logging.handlers import TimedRotatingFileHandler
import os
import re
import signal
import sys

from raven.conf import setup_logging
from raven.handlers.logging import SentryHandler

import opendiamond
from opendiamond.blobcache import ExecutableBlobCache
from opendiamond.helpers import daemonize, signalname
from opendiamond.rpc import RPCConnection, ConnectionFailure
from opendiamond.server.child import ChildManager
from opendiamond.server.listen import ConnListener
from opendiamond.server.search import Search

SEARCH_LOG_DATE_FORMAT = '%Y-%m-%d-%H:%M:%S'
SEARCH_LOG_FORMAT = 'search-%s-%d.log'          # Args: date, pid
SEARCH_LOG_REGEX = r'search-(.+)-[0-9]+\.log$'  # Match group: timestamp

_log = logging.getLogger(__name__)


class _Signalled(BaseException):
    '''Exception indicating that a signal has been received.'''

    def __init__(self, sig):
        BaseException.__init__(self)
        self.signal = sig
        self.signame = signalname(sig)


class _TimestampedLogFormatter(logging.Formatter):
    '''Format a log message with a timestamp including milliseconds delimited
    by a decimal point.'''

    def __init__(self):
        logging.Formatter.__init__(self, '%(asctime)s %(message)s',
                                   '%Y-%m-%d %H:%M:%S')

    # We're overriding a method; we can't control its name
    # pylint: disable=invalid-name
    def formatTime(self, record, datefmt=None):
        s = datetime.fromtimestamp(record.created).strftime(datefmt)
        return s + '.%03d' % record.msecs
    # pylint: enable=invalid-name


class DiamondServer(object):
    caught_signals = (signal.SIGINT, signal.SIGTERM, signal.SIGUSR1)

    def __init__(self, config):
        # Daemonize before doing any other setup
        if config.daemonize:
            daemonize()

        self.config = config
        self._children = ChildManager(config.cgroupdir, not config.oneshot)
        self._listener = ConnListener(config.diamondd_port)
        self._last_log_prune = datetime.fromtimestamp(0)
        self._last_cache_prune = datetime.fromtimestamp(0)
        self._ignore_signals = False

        # Configure signals
        for sig in self.caught_signals:
            signal.signal(sig, self._handle_signal)

        # Configure logging
        baselog = logging.getLogger()
        baselog.setLevel(config.loglevel)
        if not config.daemonize:
            # In daemon mode, stderr goes to /dev/null, so don't bother
            # logging there.
            handler = logging.StreamHandler()
            baselog.addHandler(handler)
        self._logfile_handler = TimedRotatingFileHandler(
            os.path.join(config.logdir, 'diamondd.log'), when='midnight',
            backupCount=config.logdays)
        self._logfile_handler.setFormatter(_TimestampedLogFormatter())
        baselog.addHandler(self._logfile_handler)

    # We intentionally catch all exceptions
    # pylint doesn't understand the conditional return in ConnListener.accept()
    # pylint: disable=broad-except,unpacking-non-sequence
    def run(self):
        try:
            # Log startup of parent
            _log.info('Starting supervisor %s, pid %d',
                      opendiamond.__version__, os.getpid())
            _log.info('Server IDs: %s', ', '.join(self.config.serverids))
            if self.config.cache_server:
                _log.info('Cache: %s:%d', *self.config.cache_server)
            while True:
                # Check for search logs that need to be pruned
                self._prune_child_logs()
                # Check for blob cache objects that need to be pruned
                self._prune_blob_cache()
                # Accept a new connection pair
                control, data = self._listener.accept()
                # Fork a child for this connection pair.  In the child, this
                # does not return.
                self._children.start(self._child, control, data)
                # Close the connection pair in the parent
                control.close()
                data.close()
        except _Signalled as s:
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
    # pylint: enable=broad-except,unpacking-non-sequence

    # We intentionally catch all exceptions
    # pylint: disable=broad-except
    def _child(self, control, data):
        '''Main function for child process.'''
        # Close supervisor log, open child log
        baselog = logging.getLogger()
        baselog.removeHandler(self._logfile_handler)
        del self._logfile_handler
        now = datetime.now().strftime(SEARCH_LOG_DATE_FORMAT)
        logname = SEARCH_LOG_FORMAT % (now, os.getpid())
        logpath = os.path.join(self.config.logdir, logname)
        handler = logging.FileHandler(logpath)
        handler.setFormatter(_TimestampedLogFormatter())
        baselog.addHandler(handler)

        if self.config.sentry_dsn:
            sentry_handler = SentryHandler(self.config.sentry_dsn)
            sentry_handler.setLevel(logging.ERROR)
            setup_logging(sentry_handler)

        # Okay, now we have logging
        search = None
        try:
            try:
                # Close listening socket and half-open connections
                self._listener.shutdown()
                # Log startup of child
                _log.info('Starting search %s, pid %d',
                          opendiamond.__version__, os.getpid())
                _log.info('Peer: %s', control.getpeername()[0])
                _log.info('Worker threads: %d', self.config.threads)
                # Set up connection wrappers and search object
                control = RPCConnection(control)
                search = Search(self.config, RPCConnection(data))
                # Dispatch RPCs on the control connection until we die
                while True:
                    control.dispatch(search)
            finally:
                # Ensure that further signals (particularly SIGUSR1 from
                # worker threads) don't interfere with the shutdown process.
                self._ignore_signals = True
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
            logging.shutdown()
    # pylint: enable=broad-except

    def _prune_child_logs(self):
        '''Remove search logs older than the configured number of days.'''
        # Do this check no more than once an hour
        if datetime.now() - self._last_log_prune < timedelta(hours=1):
            return
        self._last_log_prune = datetime.now()
        threshold = datetime.now() - timedelta(days=self.config.logdays)
        pattern = re.compile(SEARCH_LOG_REGEX)
        count = 0
        for file in os.listdir(self.config.logdir):
            # First check the timestamp in the file name.  This prevents
            # us from having to stat a bunch of log files that we aren't
            # interesting in GCing anyway.
            match = pattern.match(file)
            if match is None:
                continue
            try:
                start = datetime.strptime(match.group(1),
                                          SEARCH_LOG_DATE_FORMAT)
            except ValueError:
                continue
            if start >= threshold:
                continue
            # Now examine the file's mtime to ensure we're not deleting logs
            # from long-running searches
            path = os.path.join(self.config.logdir, file)
            try:
                if datetime.fromtimestamp(os.stat(path).st_mtime) < threshold:
                    os.unlink(path)
                    count += 1
            except OSError:
                pass
        if count > 0:
            _log.info('Pruned %d search logs', count)

    def _prune_blob_cache(self):
        '''Remove blob cache entries older than the configured number of
        days.'''
        # Do this check no more than once an hour
        if datetime.now() - self._last_cache_prune < timedelta(hours=1):
            return
        self._last_cache_prune = datetime.now()
        ExecutableBlobCache.prune(self.config.cachedir,
                                  self.config.blob_cache_days)

    def _handle_signal(self, sig, _frame):
        '''Signal handler in the supervisor.'''
        if not self._ignore_signals:
            raise _Signalled(sig)
