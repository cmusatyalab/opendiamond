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

from opendiamond.protocol import XDR_search_stats, XDR_filter_stats, XDR_stat

_log = logging.getLogger(__name__)


class _AggregateInterface(object):
    '''An interface for generic statistics aggregation'''

    def advance(self, value):
        raise NotImplementedError

    def finalize(self):
        raise NotImplementedError


class _Sum(_AggregateInterface):
    def __init__(self):
        super(_Sum, self).__init__()
        self._value = 0

    def advance(self, value):
        self._value += value

    def finalize(self):
        return self._value


class _Min(_AggregateInterface):
    def __init__(self):
        super(_Min, self).__init__()
        self._value = float("inf")

    def advance(self, value):
        self._value = min(self._value, value)

    def finalize(self):
        try:
            return int(self._value)
        except ArithmeticError:
            return -1


class _Max(_AggregateInterface):
    def __init__(self):
        super(_Max, self).__init__()
        self._value = float("-inf")

    def advance(self, value):
        self._value = max(self._value, value)

    def finalize(self):
        try:
            return int(self._value)
        except ArithmeticError:
            return -1


class _Avg(_AggregateInterface):
    def __init__(self):
        super(_Avg, self).__init__()
        self._sum = _Sum()
        self._count = _Sum()

    def advance(self, value):
        self._sum.advance(value)
        self._count.advance(1)

    def finalize(self):
        try:
            return int(self._sum.finalize() / self._count.finalize())
        except ZeroDivisionError:
            return 0
        except ArithmeticError:
            return -1


class _Statistics(object):
    '''Base class for server statistics.'''

    label = 'Unconfigured statistics'
    attrs = ()

    def __init__(self):
        self._lock = threading.Lock()
        self._stats = dict([(name, cls()) for name, _desc, cls in self.attrs])

    def __getattr__(self, key):
        assert isinstance(self._stats[key], _AggregateInterface)
        return self._stats[key].finalize()

    def update(self, *args, **kwargs):
        '''Atomically add 1 to the statistics listed in *args and add the
        values specified in **kwargs to the corresponding statistics.'''
        with self._lock:
            for name in args:
                self._stats[name].advance(1)
            for name, value in kwargs.iteritems():
                self._stats[name].advance(value)

    def log(self):
        '''Dump all statistics to the log.'''
        _log.info('%s:', self.label)
        for name, desc, _cls in self.attrs:
            _log.info('  %s: %d', desc, getattr(self, name))


class SearchStatistics(_Statistics):
    '''Statistics for the search as a whole.'''

    label = 'Search statistics'
    attrs = (('objs_processed', 'Objects considered', _Sum),
             ('objs_dropped', 'Objects dropped', _Sum),
             ('objs_passed', 'Objects passed', _Sum),
             ('objs_unloadable', 'Objects failing to load', _Sum),
             ('execution_us', 'Total object examination time (us)', _Sum),
             ('time_to_first_result', 'Time to get first result (us)', _Min))

    def xdr(self, objs_total, filter_stats):
        '''Return an XDR statistics structure for these statistics.'''
        with self._lock:
            try:
                avg_obj_us = self.execution_us / self.objs_processed
            except ZeroDivisionError:
                avg_obj_us = 0

            stats = []
            stats.append(XDR_stat('objs_total', objs_total))
            stats.append(XDR_stat('avg_obj_time_us', avg_obj_us))
            for name, _desc, _cls in self.attrs:
                if name != 'execution_us':
                    stats.append(XDR_stat(name, getattr(self, name)))

            return XDR_search_stats(
                stats=stats,
                filter_stats=[s.xdr() for s in filter_stats],
            )


class FilterStatistics(_Statistics):
    '''Statistics for the execution of a single filter.'''

    attrs = (('objs_processed', 'Total objects considered', _Sum),
             ('objs_dropped', 'Total objects dropped', _Sum),
             ('objs_cache_dropped', 'Objects dropped by cache', _Sum),
             ('objs_cache_passed', 'Objects skipped by cache', _Sum),
             ('objs_computed', 'Objects examined by filter', _Sum),
             ('objs_terminate', 'Objects causing filter to terminate', _Sum),
             ('execution_us', 'Filter execution time (us)', _Sum),
             ('startup_us_avg', 'Startup time (us)', _Avg))

    def __init__(self, name):
        _Statistics.__init__(self)
        self.name = name
        self.label = 'Filter statistics for %s' % name

    def xdr(self):
        '''Return an XDR statistics structure for these statistics.'''
        with self._lock:
            try:
                avg_exec_us = self.execution_us / self.objs_processed
            except ZeroDivisionError:
                avg_exec_us = 0

            stats = []
            stats.append(XDR_stat('avg_exec_time_us', avg_exec_us))
            for name, _desc, _cls in self.attrs:
                if name != 'execution_us':
                    stats.append(XDR_stat(name, getattr(self, name)))

            return XDR_filter_stats(
                name=self.name,
                stats=stats
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
        '''Elapsed time in us.'''
        return int(self.elapsed_seconds * 1e6)

