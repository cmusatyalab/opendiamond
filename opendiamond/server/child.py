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

'''Forking and monitoring of search processes by supervisor.'''

from builtins import range
from builtins import object
import logging
import os
import shutil
import signal
import sys
from tempfile import mkdtemp
import time

from opendiamond.helpers import signalname

_log = logging.getLogger(__name__)


class _SearchChild(object):
    '''A forked search process.'''

    def __init__(self, cgroupdir=None, fork=True):
        self.pid = None
        self.tempdir = mkdtemp(prefix='diamond-search-')
        self._started = False
        self._terminated = False
        self._fork = fork
        # Don't do cgroup handling if we're not forking, since there will
        # be no one to clean up the cgroup
        if fork and cgroupdir is not None:
            self._cgroupdir = mkdtemp(dir=cgroupdir, prefix='diamond-')
            self._taskfile = os.path.join(self._cgroupdir, 'tasks')
        else:
            self._cgroupdir = None
            self._taskfile = None

    def start(self):
        '''Fork off the child and return pid in the parent, 0 in the child.'''
        assert not self._started
        self._started = True

        if self._fork:
            self.pid = os.fork()
        else:
            # Act like we're in the child
            _log.info('Oneshot mode, running search in child')
            self.pid = 0

        if self.pid == 0:
            # Child
            os.environ['TMPDIR'] = self.tempdir
            # Move ourselves into a dedicated cgroup if available
            if self._taskfile is not None:
                open(self._taskfile, 'w').write('%d\n' % os.getpid())
        else:
            _log.info('Launching PID %d', self.pid)

        return self.pid

    def cleanup(self):
        '''Clean up the process' temporary directory.  If cgroups are
        enabled, also kill the process (if still running) and all of its
        descendents.'''
        if not self._terminated:
            self._terminated = True

            # Kill child plus grandchildren, great-grandchildren, etc.
            if self._taskfile is not None:
                killed = set()
                # Loop until all grandchildren are killed
                while True:
                    with open(self._taskfile) as fh:
                        pids = [int(pid) for pid in fh
                                if int(pid) not in killed]
                    if not pids:
                        break
                    for pid in pids:
                        try:
                            os.kill(pid, signal.SIGKILL)
                        except OSError:
                            # Unable to signal process; perhaps it has
                            # already died
                            pass
                        killed.add(pid)
                for _i in range(3):
                    # rmdir has been seen to return "device or resource busy"
                    # or "interrupted system call".  The former can be
                    # returned when there are processes remaining in a cgroup,
                    # so this may indicate a race within the cgroup
                    # implementation.
                    try:
                        os.rmdir(self._cgroupdir)
                    except OSError:
                        time.sleep(0.01)
                    else:
                        break
                else:
                    # Leak an empty cgroup
                    _log.warning("Couldn't remove %s", self._cgroupdir)
                _log.info('PID %d exiting, killed %d processes',
                          self.pid, len(killed))
            else:
                _log.info('PID %d cleanup', self.pid)

            # Delete temporary directory
            shutil.rmtree(self.tempdir, True)


class ChildManager(object):
    '''The set of forked search processes.'''

    def __init__(self, cgroupdir=None, fork=True):
        self._children = dict()
        self._cgroupdir = cgroupdir
        self._fork = fork
        signal.signal(signal.SIGCHLD, self._child_exited)

    def start(self, child_function, *args, **kwargs):
        '''Launch a new search process.'''
        child = _SearchChild(self._cgroupdir, self._fork)
        pid = child.start()
        if pid != 0:
            # Parent
            self._children[pid] = child
        else:
            # Child
            try:
                # Reset SIGCHLD handler
                signal.signal(signal.SIGCHLD, signal.SIG_DFL)
                # Run the child function
                child_function(*args, **kwargs)
            finally:
                # The child must never return
                sys.exit(0)

    def _cleanup_child(self, pid):
        '''Clean up the specified search process.'''
        try:
            child = self._children.pop(pid)
            child.cleanup()
        except KeyError:
            pass

    def _child_exited(self, _sig, _frame):
        '''Signal handler for SIGCHLD.'''
        while True:
            try:
                pid, status = os.waitpid(-1, os.WNOHANG)
            except OSError:
                # No child processes
                break
            if pid == 0:
                # No exited processes
                break
            if os.WIFSIGNALED(status):
                _log.info('PID %d exited on %s',
                          pid, signalname(os.WTERMSIG(status)))
            else:
                _log.info('PID %d exited with status %d',
                          pid, os.WEXITSTATUS(status))
            self._cleanup_child(pid)

    def kill_all(self):
        '''Clean up all forked search processes.'''
        for pid in list(self._children.keys()):
            _log.debug('Killing PID %d', pid)
            self._cleanup_child(pid)
