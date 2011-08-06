#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Statistics tracking.'''

from __future__ import with_statement
import logging
import threading
import time

from opendiamond.server.protocol import XDR_search_stats, XDR_filter_stats

_log = logging.getLogger(__name__)

class _Statistics(object):
    '''Base class for server statistics.'''

    label = 'Unconfigured statistics'
    attrs = ()

    def __init__(self):
        self._lock = threading.Lock()
        self._stats = dict([(name, 0) for name, _desc in self.attrs])

    def __getattr__(self, key):
        return self._stats[key]

    def update(self, *args, **kwargs):
        '''Atomically add 1 to the statistics listed in *args and add the
        values specified in **kwargs to the corresponding statistics.'''
        with self._lock:
            for name in args:
                self._stats[name] += 1
            for name, value in kwargs.iteritems():
                self._stats[name] += value

    def log(self):
        '''Dump all statistics to the log.'''
        _log.info('%s:', self.label)
        for name, desc in self.attrs:
            _log.info('  %s: %d', desc, self._stats[name])


class SearchStatistics(_Statistics):
    '''Statistics for the search as a whole.'''

    label = 'Search statistics'
    attrs = (('objs_processed', 'Objects considered'),
            ('objs_dropped', 'Objects dropped'),
            ('objs_passed', 'Objects passed'),
            ('objs_unloadable', 'Objects failing to load'),
            ('execution_ns', 'Total object examination time (ns)'))

    def xdr(self, objs_total, filter_stats):
        '''Return an XDR statistics structure for these statistics.'''
        with self._lock:
            try:
                avg_obj_time = self.execution_ns / self.objs_processed
            except ZeroDivisionError:
                avg_obj_time = 0
            return XDR_search_stats([s.xdr() for s in filter_stats],
                objs_total=objs_total,
                objs_processed=self.objs_processed,
                objs_dropped=self.objs_dropped,
                objs_nproc=self.objs_passed,
                avg_obj_time=avg_obj_time,
            )


class FilterStatistics(_Statistics):
    '''Statistics for the execution of a single filter.'''

    attrs = (('objs_processed', 'Total objects considered'),
            ('objs_dropped', 'Total objects dropped'),
            ('objs_cache_dropped', 'Objects dropped by cache'),
            ('objs_cache_passed', 'Objects skipped by cache'),
            ('objs_compute', 'Objects examined by filter'),
            ('objs_terminate', 'Objects causing filter to terminate'),
            ('execution_ns', 'Filter execution time (ns)'))

    def __init__(self, name):
        _Statistics.__init__(self)
        self.name = name
        self.label = 'Filter statistics for %s' % name

    def xdr(self):
        '''Return an XDR statistics structure for these statistics.'''
        with self._lock:
            try:
                avg_exec_time = self.execution_ns / self.objs_processed
            except ZeroDivisionError:
                avg_exec_time = 0
            return XDR_filter_stats(self.name,
                objs_processed=self.objs_processed,
                objs_dropped=self.objs_dropped,
                objs_cache_dropped=self.objs_cache_dropped,
                objs_cache_passed=self.objs_cache_passed,
                objs_compute=self.objs_compute,
                avg_exec_time=avg_exec_time,
            )


class Timer(object):
    '''Tracks the elapsed time since the Timer object was created.'''

    def __init__(self):
        self._start = time.time()

    @property
    def elapsed_seconds(self):
        '''Elapsed time in seconds.'''
        return time.time() - self._start

    @property
    def elapsed(self):
        '''Elapsed time in ns.'''
        return int(self.elapsed_seconds * 1e9)
