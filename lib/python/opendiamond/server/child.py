#
#  The OpenDiamond Platform for Interactive Search
#  Version 6
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Forking and monitoring of search processes by supervisor.'''

import logging
import os
import shutil
import signal
import sys
from tempfile import mkdtemp

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
            os.putenv('TMPDIR', self.tempdir)
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
                    pids = [int(pid) for pid in open(self._taskfile)]
                    if len(pids) == 0:
                        break
                    for pid in pids:
                        if pid not in killed:
                            os.kill(pid, signal.SIGKILL)
                            killed.add(pid)
                os.rmdir(self._cgroupdir)
                _log.info('PID %d exiting, killed %d processes', self.pid,
                                        len(killed))
            else:
                _log.info('PID %d exiting', self.pid)

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

    def _child_exited(self, sig, frame):
        '''Signal handler for SIGCHLD.'''
        try:
            while True:
                pid, status = os.waitpid(-1, os.WNOHANG)
                if pid == 0:
                    # No exited processes
                    break
                _log.debug('PID %d exited', pid)
                self._cleanup_child(pid)
        except OSError:
            # No child processes
            return

    def kill_all(self):
        '''Clean up all forked search processes.'''
        for pid in self._children.keys():
            _log.debug('Killing PID %d', pid)
            self._cleanup_child(pid)
