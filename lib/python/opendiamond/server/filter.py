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

import logging
import os
import signal
import subprocess
import threading

from opendiamond.server.rpc import ConnectionFailure
from opendiamond.server.statistics import FilterStatistics, Timer

_log = logging.getLogger(__name__)

class FilterSpecError(Exception):
    pass
class FilterDependencyError(Exception):
    pass
class FilterExecutionError(Exception):
    pass


class _FilterProcess(object):
    '''A connection to a running filter process.'''
    def __init__(self, path, name, args, blob):
        try:
            self._proc = subprocess.Popen([path, '--filter'],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                close_fds=True, cwd=os.getenv('TMPDIR'))
            self._fin = self._proc.stdout
            self._fout = self._proc.stdin

            # Send:
            # - Protocol version (1)
            # - Filter name
            # - Array of filter arguments
            # - Blob argument
            self.send(1, name, args, blob)
        except OSError, IOError:
            raise FilterExecutionError('Unable to launch filter %s' % name)

    def __del__(self):
        if self._proc.poll() is None:
            os.kill(self._proc.pid, signal.SIGKILL)
            self._proc.wait()

    def get_tag(self):
        '''Read and return a tag.'''
        return self._fin.readline().strip()

    def get_item(self):
        '''Read and return a string or blob.'''
        sizebuf = self._fin.readline()
        if len(sizebuf) == 0:
            # End of file
            raise IOError('End of input stream')
        elif len(sizebuf.strip()) == 0:
            # No length value == no data
            return None
        size = int(sizebuf, 10)
        item = self._fin.read(size)
        if len(item) != size:
            raise IOError('Short read from stream')
        # Swallow trailing newline
        self._fin.read(1)
        return item

    def get_array(self):
        '''Read and return an array of strings or blobs.'''
        arr = []
        while True:
            str = self.get_item()
            if str is None:
                return arr
            arr.append(str)

    def send(self, *values):
        '''Send one or more values.  An argument can be:
           boolean => serialized to "true" or "false"
           None => serialized as a blank line
           scalar => serialized as str(value)
           tuple or list => serialized as an array terminated by a blank line
        '''
        def send_value(value):
            value = str(value)
            self._fout.write('%d\n%s\n' % (len(value), value))
        for value in values:
            if isinstance(value, list) or isinstance(value, tuple):
                for element in value:
                    send_value(element)
                self._fout.write('\n')
            elif value is True:
                send_value('true')
            elif value is False:
                send_value('false')
            elif value is None:
                self._fout.write('\n')
            else:
                send_value(value)
        self._fout.flush()


class _FilterResult(object):
    def __init__(self):
        self.input_attrs = {}	# name -> MD5(value)
        self.output_attrs = {}	# name -> MD5(value)
        self.score = 0


class _FilterRunner(object):
    '''A context for processing objects with a Filter.'''
    def __init__(self, state, filter, code_path):
        self._filter = filter
        self._state = state
        self._code_path = code_path
        self._proc = None
        self._proc_initialized = False

    def __str__(self):
        return self._filter.name

    def evaluate(self, obj):
        if self._proc is None:
            self._proc = _FilterProcess(self._code_path, self._filter.name,
                                    self._filter.arguments, self._filter.blob)
            self._proc_initialized = False
        timer = Timer()
        result = _FilterResult()
        proc = self._proc
        try:
            while True:
                cmd = proc.get_tag()
                if cmd == 'init-success':
                    # The filter initialized successfully.  This may not
                    # be the first command produced by the filter, since
                    # its init function may e.g. produce log messages.
                    self._proc_initialized = True
                elif cmd == 'get-attribute':
                    key = proc.get_item()
                    if key in obj:
                        proc.send(obj[key])
                        result.input_attrs[key] = obj.get_signature(key)
                    else:
                        proc.send(None)
                elif cmd == 'set-attribute':
                    key = proc.get_item()
                    value = proc.get_item()
                    obj[key] = value
                    result.output_attrs[key] = obj.get_signature(key)
                elif cmd == 'omit-attribute':
                    key = proc.get_item()
                    try:
                        obj.omit(key)
                        proc.send(True)
                    except KeyError:
                        proc.send(False)
                elif cmd == 'get-session-variables':
                    keys = proc.get_array()
                    valuemap = self._state.session_vars.filter_get(keys)
                    values = [valuemap[key] for key in keys]
                    proc.send(values)
                elif cmd == 'update-session-variables':
                    keys = proc.get_array()
                    values = proc.get_array()
                    try:
                        values = [float(f) for f in values]
                    except ValueError:
                        raise FilterExecutionError(
                                    '%s: bad session variable value' % self)
                    if len(keys) != len(values):
                        raise FilterExecutionError(
                                    '%s: bad array lengths' % self)
                    valuemap = dict(zip(keys, values))
                    self._state.session_vars.filter_update(valuemap)
                elif cmd == 'log':
                    level = int(proc.get_item(), 10)
                    message = proc.get_item()
                    if level & 0x01:
                        # LOGL_CRIT
                        level = logging.CRITICAL
                    elif level & 0x02:
                        # LOGL_ERR
                        level = logging.ERROR
                    elif level & 0x04:
                        # LOGL_INFO
                        level = logging.INFO
                    elif level & 0x08:
                        # LOGL_TRACE.  Very verbose; ignore.
                        continue
                    elif level & 0x10:
                        # LOGL_DEBUG
                        level = logging.DEBUG
                    else:
                        level = logging.DEBUG
                    _log.log(level, message)
                elif cmd == 'stdout':
                    print proc.get_item(),
                elif cmd == 'result':
                    result.score = int(proc.get_item(), 10)
                    break
                else:
                    raise FilterExecutionError('%s: unknown command' % self)
        except IOError:
            if self._proc_initialized:
                # Filter died on an object.  The result score defaults to
                # zero, so this will be treated as a drop.
                _log.error('Filter %s (signature %s) died on object %s',
                                self, self._filter.signature, obj.id)
                self._proc = None
            else:
                # Filter died during initialization.  Treat this as fatal.
                raise FilterExecutionError("Filter %s failed to initialize"
                                % name)
        finally:
            accept = self.threshold(result)
            self._filter.stats.update('objs_processed', 'objs_compute',
                                    objs_dropped=int(not accept),
                                    execution_ns=timer.elapsed)
        return result

    def threshold(self, result):
        return result.score >= self._filter.threshold


class Filter(object):
    '''A filter with arguments.'''
    def __init__(self, name, signature, threshold, arguments, dependencies):
        self.name = name
        self.signature = signature
        self.threshold = threshold
        self.arguments = arguments
        self.dependencies = dependencies
        self.stats = FilterStatistics(name)
        # Additional state that needs to be set later by the caller
        self.blob = ''

    @classmethod
    def from_fspec(cls, fspec_lines):
        name = None
        signature = None
        threshold = None
        arguments = []
        dependencies = []

        # The fspec format previously allowed comments, including at
        # end-of-line, but modern fspecs are all produced programmatically by
        # OpenDiamond-Java which never includes any.
        for line in fspec_lines:
            k, v = line.split(None, 1)
            v = v.strip()
            if k == 'FILTER':
                name = v
                if name == 'APPLICATION':
                    # The FILTER APPLICATION stanza specifies "application
                    # dependencies", which are a legacy construct.
                    # Ignore these.
                    return None
            elif k == 'ARG':
                arguments.append(v)
            elif k == 'THRESHOLD':
                try:
                    threshold = int(v, 10)
                except ValueError:
                    raise FilterSpecError('Threshold not an integer')
            elif k == 'SIGNATURE':
                signature = v
            elif k == 'REQUIRES':
                dependencies.append(v)
            elif k == 'MERIT':
                # Deprecated
                pass
            else:
                raise FilterSpecError('Unknown fspec key %s' % k)

        if name is None or signature is None or threshold is None:
            raise FilterSpecError('Missing mandatory fspec key')
        return cls(name, signature, threshold, arguments, dependencies)

    def bind(self, state):
        '''Returns a _FilterRunner for this filter.'''
        try:
            code_path = state.blob_cache.path(self.signature)
        except KeyError:
            raise FilterExecutionError('Missing code for filter ' + self.name)
        return _FilterRunner(state, self, code_path)


class FilterStackRunner(threading.Thread):
    def __init__(self, state, filter_runners, name):
        threading.Thread.__init__(self, name=name)
        self.setDaemon(True)
        self._state = state
        self._runners = filter_runners

    def evaluate(self, obj):
        obj.load()
        timer = Timer()
        accept = False
        try:
            for runner in self._runners:
                result = runner.evaluate(obj)
                if not runner.threshold(result):
                    break
            else:
                accept = True
        finally:
            self._state.stats.update('objs_processed',
                                    execution_ns=timer.elapsed,
                                    objs_passed=accept and 1 or 0,
                                    objs_dropped=(not accept) and 1 or 0)
        return accept

    def run(self):
        '''Thread function.'''
        try:
            # ScopeListLoader properly handles interleaved access by
            # multiple threads
            for obj in self._state.scope:
                if self.evaluate(obj):
                    xdr = obj.xdr(self._state.search_id,
                                    self._state.push_attrs)
                    self._state.blast.send(xdr)
        except ConnectionFailure, e:
            # Client closed blast connection.  Rather than just calling
            # sys.exit(), signal the main thread to shut us down.
            _log.info('Search exiting: %s', str(e))
            os.kill(os.getpid(), signal.SIGUSR1)
        except Exception:
            _log.exception('Worker thread exception')
            os.kill(os.getpid(), signal.SIGUSR1)


class FilterStack(object):
    def __init__(self, filters=[]):
        # name -> Filter
        self._filters = dict([(f.name, f) for f in filters])
        # Ordered list of filters to execute
        self._order = list()

        # Resolve declared dependencies
        # Filters we have already resolved
        resolved = set()
        # Filters we are currently resolving
        inprocess = set()
        def resolve(filter):
            if filter in resolved:
                return
            if filter in inprocess:
                raise FilterDependencyError('Circular dependency involving '
                                    + filter.name)
            inprocess.add(filter)
            for depname in filter.dependencies:
                try:
                    resolve(self._filters[depname])
                except KeyError:
                    raise FilterDependencyError('No such filter: ' + depname)
            inprocess.remove(filter)
            self._order.append(filter)
            resolved.add(filter)
        for filter in filters:
            resolve(filter)

    def __len__(self):
        return len(self._order)

    def __iter__(self):
        return iter(self._order)

    def __getitem__(self, name):
        '''Filter lookup by name.'''
        return self._filters[name]

    @classmethod
    def from_fspec(cls, data):
        fspec = []
        filters = []
        def add_filter(fspec):
            if len(fspec) > 0:
                filter = Filter.from_fspec(fspec)
                if filter is not None:
                    filters.append(filter)
        for line in data.split('\n'):
            if line.strip() == '':
                continue
            if line.startswith('FILTER'):
                add_filter(fspec)
                fspec = []
            fspec.append(line)
        add_filter(fspec)
        return cls(filters)

    def bind(self, state, name='Filter'):
        '''Returns a FilterStackRunner that can be used to process objects
        with this filter stack.'''
        return FilterStackRunner(state, [f.bind(state) for f in self._order],
                                name)

    def start_threads(self, state, count):
        for i in xrange(count):
            self.bind(state, 'Filter-%d' % i).start()
