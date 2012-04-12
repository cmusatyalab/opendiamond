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

'''Filter configuration and execution; result and attribute caching.

There are two caches, both accessible via key lookups in the same
Redis database.

Result cache:
    'result:' + MD5(
        ' '.join(
            filter signature,
            ' '.join(filter arguments),
            filter blob argument,
            object ID
        )
    ) => JSON({
        'input_attrs': {attribute name => MD5(attribute value)},
        'output_attrs': {attribute name => MD5(attribute value)},
        'score': filter score
    })

Attribute cache:
    'attribute:' + MD5(attribute value) => attribute value

The purpose of the result cache is to reuse drop decisions without needing
to rerun any filters.  A result cache lookup on an object returns an array
of FilterResult entries, one for each filter in the filter stack, where one
or more entries may be null.  The score recorded in each result is compared
to the corresponding threshold for the current search.  If one or more
results produces a drop decision, the dependency chain of attribute values
for that result is traversed to prove that its input attribute values were
produced by filters present in, and identically configured in, the current
search.  If so, the result cache drops the object.  Otherwise, we proceed to
filter execution.  FilterResult objects produced by filter execution are
always stored in the result cache.

The attribute cache is used to reduce the number of filters we need to run
when executing a filter stack on an object.  When preparing to run a filter,
we compare its cached FilterResult (if any) to the actual contents of the
object to ensure that the cached result's input attribute dependencies are
met.  If so, we look up the hashes of the cached output values in the
attribute cache.  If they are present, we store those values in the object
and skip execution of the filter.  Otherwise, we execute the filter.  To
avoid storing cheaply recomputable values in the attribute cache, we only
cache values resulting from filter executions that produce attribute data at
less than 2 MB/s.
'''

import logging
import os
from redis import Redis
from redis.exceptions import ResponseError
import signal
import simplejson as json
import subprocess
import threading

from opendiamond.helpers import md5, signalname, split_scheme
from opendiamond.rpc import ConnectionFailure
from opendiamond.server.object_ import ObjectLoader, ObjectLoadError
from opendiamond.server.statistics import FilterStatistics, Timer

ATTR_FILTER_SCORE = '_filter.%s_score'	# arg: filter name
# If a filter produces attribute values at less than this rate
# (total attribute value size / execution time), we will cache the attribute
# values as well as the filter results.
ATTRIBUTE_CACHE_THRESHOLD = 2 << 20	# bytes/sec
DEBUG = False

_log = logging.getLogger(__name__)
if DEBUG:
    _debug = _log.debug
else:
    _debug = lambda *args, **kwargs: None

class FilterDependencyError(Exception):
    '''Error processing filter dependencies.'''
class FilterExecutionError(Exception):
    '''Error executing filter.'''
class FilterUnsupportedSource(Exception):
    '''URI scheme for code or blob source is not supported.'''
class _DropObject(Exception):
    '''Filter failed to process object.  The object should be dropped
    without caching the drop result.'''


class _FilterProcess(object):
    '''A connection to a running filter process.'''
    def __init__(self, code_argv, name, args, blob):
        try:
            self._name = name
            self._proc = subprocess.Popen(code_argv + ['--filter'],
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
        except (OSError, IOError):
            raise FilterExecutionError('Unable to launch filter %s' % self)

    def __del__(self):
        ret = self._proc.poll()
        if ret is None:
            os.kill(self._proc.pid, signal.SIGKILL)
            self._proc.wait()
        elif ret < 0:
            _log.info('Filter %s exited on %s', self, signalname(-ret))
        elif ret > 0:
            _log.info('Filter %s exited with status %d', self, ret)

    def __str__(self):
        return self._name

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
        size = int(sizebuf)
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
    '''A summary of the result of running a filter on an object: the score
    and hashes of the output attributes, together with hashes of the input
    attributes used to produce them.'''

    def __init__(self, input_attrs=None, output_attrs=None, score=0.0):
        self.input_attrs = input_attrs or {}	# name -> MD5(value)
        self.output_attrs = output_attrs or {}	# name -> MD5(value)
        self.score = score
        # Whether to cache output attributes in the attribute cache
        self.cache_output = False

    def encode(self):
        return json.dumps({
            'input_attrs': self.input_attrs,
            'output_attrs': self.output_attrs,
            'score': self.score,
        })

    @classmethod
    def decode(cls, data):
        if data is None:
            return None
        dct = json.loads(data)
        try:
            return cls(dct['input_attrs'], dct['output_attrs'], dct['score'])
        except KeyError:
            return None


class _ObjectProcessor(object):
    '''A context for processing objects.'''

    # Whether to report the filter score back to the client (True for
    # filters requested by the client, False for other filters)
    send_score = False

    def __str__(self):
        '''Return a human-readable name for the underlying filter.'''
        raise NotImplementedError()

    def get_cache_key(self, obj):
        '''Return the result cache lookup key for previous filter executions
        on this object.'''
        digest = self._get_cache_digest()
        digest.update(str(obj))
        return 'result:' + digest.hexdigest()

    def _get_cache_digest(self):
        '''Return a digest object with object-independent information about
        the filter (e.g. its arguments) already hashed into it.'''
        raise NotImplementedError()

    def cache_hit(self, result):
        '''Notification callback that an object has hit in the cache,
        producing the given result.'''
        pass

    def evaluate(self, obj):
        '''Execute the filter on this object, returning a _FilterResult.'''
        raise NotImplementedError()

    def threshold(self, result):
        '''Apply the drop threshold to the _FilterResult and return True
        to accept the object or False to drop it.'''
        raise NotImplementedError()


class _ObjectFetcher(_ObjectProcessor):
    '''A context for loading object data from the dataretriever.'''

    def __init__(self, state):
        _ObjectProcessor.__init__(self)
        self._state = state
        self._loader = ObjectLoader(state.config, state.blob_cache)
        self._digest_prefix = md5('dataretriever ')

    def __str__(self):
        return 'fetcher'

    def _get_cache_digest(self):
        return self._digest_prefix.copy()

    def evaluate(self, obj):
        try:
            self._loader.load(obj)
        except ObjectLoadError, e:
            _log.warning('Failed to load %s: %s', obj, e)
            self._state.stats.update('objs_unloadable')
            raise _DropObject()
        result = _FilterResult()
        for key in obj:
            result.output_attrs[key] = obj.get_signature(key)
        return result

    def threshold(self, result):
        return True


class _FilterRunner(_ObjectProcessor):
    '''A context for processing objects with a Filter.'''

    send_score = True

    def __init__(self, state, filter):
        _ObjectProcessor.__init__(self)
        self._filter = filter
        self._state = state
        self._proc = None
        self._proc_initialized = False

    def __str__(self):
        return self._filter.name

    def _get_cache_digest(self):
        return self._filter.get_cache_digest()

    def cache_hit(self, result):
        accept = self.threshold(result)
        self._filter.stats.update('objs_processed',
                                objs_dropped=int(not accept),
                                objs_cache_dropped=int(not accept),
                                objs_cache_passed=int(accept))

    def evaluate(self, obj):
        if self._proc is None:
            debug = self._state.config.debug_filters
            if self._filter.name in debug or self._filter.signature in debug:
                argv = (self._state.config.debug_command +
                            [self._filter.code_path])
            else:
                argv = [self._filter.code_path]
            self._proc = _FilterProcess(argv, self._filter.name,
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
                    level = int(proc.get_item())
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
                    result.score = float(proc.get_item())
                    break
                elif cmd == '':
                    # Encountered EOF on pipe
                    raise IOError()
                else:
                    raise FilterExecutionError('%s: unknown command' % self)
        except IOError:
            if self._proc_initialized:
                # Filter died on an object.  Drop the object without caching
                # the result.
                _log.error('Filter %s (signature %s) died on object %s',
                                self, self._filter.signature, obj)
                self._filter.stats.update('objs_terminate')
                self._proc = None
                raise _DropObject()
            else:
                # Filter died during initialization.  Treat this as fatal.
                raise FilterExecutionError("Filter %s failed to initialize"
                                % self)
        finally:
            accept = self.threshold(result)
            self._filter.stats.update('objs_processed', 'objs_compute',
                                    objs_dropped=int(not accept),
                                    execution_ns=timer.elapsed)
            lengths = [len(obj[k]) for k in result.output_attrs]
            throughput = int(sum(lengths) / timer.elapsed_seconds)
            if throughput < ATTRIBUTE_CACHE_THRESHOLD:
                result.cache_output = True
        return result

    def threshold(self, result):
        return (result.score >= self._filter.min_score and
                result.score <= self._filter.max_score)


class Filter(object):
    '''A filter with arguments.'''

    def __init__(self, name, code_source, blob_source, min_score, max_score,
                arguments, dependencies):
        self.name = name
        self.code_source = code_source
        self.blob_source = blob_source
        self.min_score = min_score
        self.max_score = max_score
        self.arguments = arguments
        self.dependencies = dependencies
        self.stats = FilterStatistics(name)

        # Will be initialized during resolve()
        self.code_path = None
        self.signature = None
        self.blob = None
        self._digest_prefix = None

    def get_cache_digest(self):
        '''Return a digest object with information about the filter (e.g.
        its arguments) already hashed into it.'''
        return self._digest_prefix.copy()

    def resolve(self, state):
        '''Ensure filter code and blob argument are available in the blob
        cache, load the blob argument, and initialize the cache digest.'''
        if self.code_path is not None:
            return
        # Get path to filter code
        code_path, signature = self._resolve_code(state)
        # Get contents of blob argument
        blob = self._resolve_blob(state)
        # Initialize digest
        summary = [signature] + self.arguments + [blob]
        digest_prefix = md5(' '.join(summary))
        # Commit
        self.code_path = code_path
        self.signature = signature
        self.blob = blob
        self._digest_prefix = digest_prefix

    def _resolve_code(self, state):
        '''Returns (code_path, signature).'''
        scheme, path = split_scheme(self.code_source)
        if scheme == state.blob_cache.digest:
            sig = path
            try:
                return (state.blob_cache.executable_path(sig), sig)
            except KeyError:
                raise FilterDependencyError('Missing code for filter ' +
                                        self.name)
        else:
            raise FilterUnsupportedSource()

    def _resolve_blob(self, state):
        '''Returns blob data.'''
        scheme, path = split_scheme(self.blob_source)
        if scheme == state.blob_cache.digest:
            try:
                return state.blob_cache[path]
            except KeyError:
                raise FilterDependencyError('Missing blob for filter ' +
                                        self.name)
        else:
            raise FilterUnsupportedSource()

    @classmethod
    def source_available(cls, state, uri):
        '''Verify the URI to ensure that its data is accessible.  Return
        True if we can access the data, False if we can't and should inform
        the client to that effect.  Raise FilterUnsupportedSource if we don't
        support the URI scheme.'''
        scheme, path = split_scheme(uri)
        if scheme == state.blob_cache.digest:
            return path in state.blob_cache
        else:
            raise FilterUnsupportedSource()

    def bind(self, state):
        '''Return a _FilterRunner for this filter.'''
        # resolve() must be called first
        assert self.code_path is not None
        return _FilterRunner(state, self)


class FilterStackRunner(threading.Thread):
    '''A context for processing objects with a FilterStack.  Handles querying
    and updating the result and attribute caches.'''

    def __init__(self, state, filter_runners, name, cleanup):
        threading.Thread.__init__(self, name=name)
        self.setDaemon(True)
        self._state = state
        self._runners = filter_runners
        self._redis = None	# May be None if caching is not enabled
        self._cleanup = cleanup	# cleanup.__del__ fires when all workers exit
        self._warned_cache_update = False

    def _get_attribute_key(self, value_sig):
        '''Return an attribute cache lookup key for the specified signature.'''
        return 'attribute:' + value_sig

    def _result_cache_can_drop(self, obj, cache_results):
        '''Return True if the object can be dropped.  cache_results is a
        runner -> _FilterResult map retrieved from the result cache.'''

        # Build output_key -> [runners] mapping.
        output_attrs = dict()
        for runner, result in cache_results.iteritems():
            for k in result.output_attrs:
                output_attrs.setdefault(k, []).append(runner)

        # Now follow the dependency chains of each runner that produced a
        # drop decision to determine whether any of them have cached results
        # we can use.  A usable cached result is one where every input
        # attribute that was used to calculate the result, directly or via a
        # dependency chain, is an output attribute of another cached result.
        # (In other words, an unusable cached result is one where we cannot
        # prove that all of its inputs match the inputs on which it was
        # originally run.)  We compute the set of filters that contributed
        # to a drop decision so that the runners can be notified to update
        # their statistics.
        resolved = dict()	# runner -> set(runner + transitive depends)
        inprocess = set()	# runner
        def resolve(runner):
            '''If this runner has usable cached results, return a set
            containing the runner and its transitive dependencies.  Otherwise
            return None.'''
            if runner in resolved:
                return resolved[runner]
            try:
                result = cache_results[runner]
            except KeyError:
                # No cached result for this runner.
                return
            if runner in inprocess:
                # Circular dependency in cache; shouldn't happen.
                # Bail out on this resolution.
                _log.error('Circular dependency in cache for object %s', obj)
                return
            inprocess.add(runner)
            try:
                dependencies = set([runner])
                # For each input attribute...
                for key, valsig in result.input_attrs.iteritems():
                    # ...try to find a resolvable filter that generated it.
                    for cur in output_attrs.get(key, ()):
                        if cache_results[cur].output_attrs[key] != valsig:
                            # This filter generated the right attribute name
                            # but the wrong attribute value.  This means that
                            # the output of the filter can vary with the value
                            # of an input (probably a filter argument) which
                            # is not captured in the result cache key of the
                            # runner, leading to a cache collision for the
                            # runner.  To fix this, filter authors should add
                            # the hash of the dependency's arguments as a
                            # dummy argument to the runner's filter.
                            _log.warning('Result cache collision for ' +
                                        'filter %s', runner)
                            continue
                        cur_deps = resolve(cur)
                        if cur_deps is not None:
                            # Resolved this input attribute.
                            dependencies.update(cur_deps)
                            break
                    else:
                        # No resolvable filter generated this attribute.
                        return
                # Successfully resolved dependencies.
                _debug('Resolved: %s', runner)
                resolved[runner] = dependencies
                return dependencies
            finally:
                inprocess.remove(runner)

        for runner, result in cache_results.iteritems():
            if not runner.threshold(result):
                # This would be a drop.  Try to resolve the cached result.
                deps = resolve(runner)
                if deps is not None:
                    # Success!  Notify runners that participated in the
                    # cached result and drop the object.
                    _debug('Drop via %s', runner)
                    for cur in deps:
                        cur.cache_hit(cache_results[cur])
                    return True
        else:
            return False

    def _attribute_cache_try_load(self, runner, obj, result):
        '''Try to update object attributes from the cached result from
        this runner, thereby avoiding the need to reexecute the filter.
        Return True if successful.'''
        for key, valsig in result.input_attrs.iteritems():
            if key not in obj or obj.get_signature(key) != valsig:
                # Input attribute is not present in the object or does
                # not have the correct hash.  We recheck the hash
                # because one of the dependent filters may have been
                # rerun (due to uncached result values) and may have
                # (improperly) produced a different output this time.
                _debug('Missing dependent value for %s: %s', runner, key)
                return False
        keys = result.output_attrs.keys()
        cache_keys = [self._get_attribute_key(result.output_attrs[k])
                        for k in keys]
        if self._redis is not None and len(cache_keys) > 0:
            values = self._redis.mget(cache_keys)
        else:
            values = [None for k in cache_keys]
        if None in values:
            # One or more attribute values was not cached.  We need
            # to rerun the filter.
            _debug('Uncached output value for %s', runner)
            return False
        else:
            _debug('Cached output values for %s', runner)
            # Load the attribute values into the object.
            for key, value in zip(keys, values):
                obj[key] = value
            # Notify the runner that it had a cache hit.
            runner.cache_hit(result)
            return True

    def _evaluate(self, obj):
        _debug('Evaluating %s', obj)

        # Calculate runner -> result cache key mapping.
        cache_keys = dict([(r, r.get_cache_key(obj)) for r in self._runners])

        # Look up all filter results in the cache and build runner -> result
        # mapping for results that exist.
        if self._redis is not None:
            keys = [cache_keys[r] for r in self._runners]
            results = [(runner, _FilterResult.decode(data))
                                for runner, data in
                                zip(self._runners, self._redis.mget(keys))]
            # runner -> _FilterResult
            cache_results = dict([(k, v) for k, v in results if v is not None])
        else:
            cache_results = dict()

        # Evaluate the object in the result cache.
        if self._result_cache_can_drop(obj, cache_results):
            return False

        new_results = dict()		# runner -> result
        try:
            # Run each filter or load its prior result into the object.
            for runner in self._runners:
                if (runner in cache_results and
                            self._attribute_cache_try_load(runner, obj,
                            cache_results[runner])):
                    result = cache_results[runner]
                else:
                    result = runner.evaluate(obj)
                    new_results[runner] = result
                if not runner.threshold(result):
                    # Drop decision.
                    return False
                elif runner.send_score:
                    # Store the filter score in the object.  This attribute
                    # is never cached because the filter name is controlled
                    # by the client and can arbitrarily change across
                    # searches.
                    attrname = ATTR_FILTER_SCORE % runner
                    obj[attrname] = str(result.score) + '\0'
            # Object passes all filters, accept
            return True
        except _DropObject:
            return False
        finally:
            # Update the cache with new values
            resultmap = dict()
            for runner, result in new_results.iteritems():
                # Result cache entry
                resultmap[cache_keys[runner]] = result.encode()
                # Attribute cache entries, if the filter was expensive enough
                if result.cache_output:
                    resultmap.update([(self._get_attribute_key(valsig),
                                            obj[key]) for key, valsig in
                                            result.output_attrs.iteritems()])
            # Do it
            if self._redis is not None and resultmap:
                try:
                    self._redis.mset(resultmap)
                except ResponseError, e:
                    # mset failed, possibly due to maxmemory quota
                    if not self._warned_cache_update:
                        self._warned_cache_update = True
                        _log.warning('Failed to update cache: %s', e)

    def evaluate(self, obj):
        '''Evaluate the object and return True to accept or False to drop.'''
        timer = Timer()
        accept = False
        try:
            accept = self._evaluate(obj)
        finally:
            self._state.stats.update('objs_processed',
                                    execution_ns=timer.elapsed,
                                    objs_passed=int(accept),
                                    objs_dropped=int(not accept))
        return accept

    def run(self):
        '''Thread function.'''
        try:
            config = self._state.config
            if config.cache_server is not None:
                host, port = config.cache_server
                self._redis = Redis(host=host, port=port,
                                    db=config.cache_database,
                                    password=config.cache_password)
                # Ensure the Redis server is available
                self._redis.ping()

            # ScopeListLoader properly handles interleaved access by
            # multiple threads
            for obj in self._state.scope:
                if self.evaluate(obj):
                    self._state.blast.send(obj)
        except ConnectionFailure:
            # Client closed blast connection.  Rather than just calling
            # sys.exit(), signal the main thread to shut us down.
            os.kill(os.getpid(), signal.SIGUSR1)
        except Exception:
            _log.exception('Worker thread exception')
            os.kill(os.getpid(), signal.SIGUSR1)


class Reference(object):
    '''When destroyed, calls the specified callback.'''

    def __init__(self, callback):
        self._callback = callback

    def __del__(self):
        self._callback()


class FilterStack(object):
    '''A set of filters which collectively decide to accept or drop an
    object.'''

    def __init__(self, filters=None):
        if filters is None:
            filters = []
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

    def bind(self, state, name='Filter', cleanup=None):
        '''Return a FilterStackRunner that can be used to process objects
        with this filter stack.'''
        fetcher = _ObjectFetcher(state)
        runners = [fetcher] + [f.bind(state) for f in self._order]
        return FilterStackRunner(state, runners, name, cleanup)

    def start_threads(self, state, count):
        '''Start count threads to process objects with this filter stack.'''
        cleanup = Reference(state.blast.close)
        for i in xrange(count):
            self.bind(state, 'Filter-%d' % i, cleanup).start()
