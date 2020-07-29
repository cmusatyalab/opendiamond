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

'''Search state; control and blast channel handling.'''

import binascii
import uuid
from collections import deque, defaultdict
from hashlib import sha256
import struct

from functools import wraps
import logging
import multiprocessing as mp
import os
import signal
import threading
import json
import io

import random
import numpy as np
import requests
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry
import time
from io import BytesIO

from opendiamond import protocol
from opendiamond.protocol import (
    DiamondRPCFailure, DiamondRPCFCacheMiss, DiamondRPCCookieExpired,
    DiamondRPCSchemeNotSupported)
from opendiamond.protocol import (
    XDR_setup, XDR_filter_config, XDR_blob_data, XDR_start, XDR_reexecute,
    DiamondRPCFCacheMiss)
from opendiamond.rpc import RPCHandlers, RPCError, RPCProcedureUnavailable
from opendiamond.scope import get_cookie_map, ScopeCookie, ScopeError, ScopeCookieExpired
from opendiamond.client.rpc import ControlConnection, BlastConnection
from opendiamond.server.object_ import EmptyObject
from opendiamond.rpc import RPCError, ConnectionFailure
from opendiamond.proxy.filter import ProxyFilter
from opendiamond.proxy.utils import load_dataset_from_zipfile
from sklearn.svm import SVC
from zipfile import ZipFile

logging.info('Starting logger for...')

_log = logging.getLogger(__name__)
_log.setLevel(logging.DEBUG)

proxy_model = None
model_lock = threading.Lock()
dropped_count = 0
true_dropped = 0
stats_lock = threading.Lock()

class ProxySearch(RPCHandlers):
    '''State for a single search, plus handlers for control channel RPCs
    to modify it.'''

    log_rpcs = True
    running = False

    def __init__(self, blast_conn, docker_address):
        RPCHandlers.__init__(self)
        self._blast_conn = blast_conn
        self.blast_start = False
        self._scope = None
        self._connections = None
        self.blob_map = {}
        self._running = False
        self.proxy_filter = None
        self.docker_address = docker_address
        self.current_stats = {}
        self.previous_stats = {'objs_passed': 0,
                           'objs_unloadable': 0,
                           'objs_dropped': 0,
                           'objs_total': 0,
                           'objs_true_positive': 0,
                           'objs_processed': 0,
                           'objs_false_negative': 0,
                           'avg_obj_time_us': 0}


    def shutdown(self):
        '''Clean up the search before the process exits.'''

        # Clean up the resource context before terminate() to avoid corrupting the shared data structures.
        ProxySearch.running = False
        if self._connections:
            for c in self._connections.values():
                c.close()
        try:
            os.kill(os.getpid(), signal.SIGKILL)
        except OSError:
            pass



    # This is not a static method: it's only called when initializing the
    # class, and the staticmethod() decorator does not create a callable.
    # Also avoid complaints about accesses to self._running
    # pylint: disable=no-self-argument,protected-access
    def running(should_be_running):
        '''Decorator that specifies that the handler can only be called
        before, or after, the search has started running.'''

        def decorator(func):
            @wraps(func)
            def wrapper(self, *args, **kwargs):
                if self._running != should_be_running:
                    raise RPCProcedureUnavailable()
                return func(self, *args, **kwargs)

            return wrapper

        return decorator

    # pylint: enable=no-self-argument,protected-access

    def _check_runnable(self):
        '''Validate state preparatory to starting a search or reexecution.'''
        if self._scope is None:
            raise DiamondRPCFailure('No search scope configured')
        if not self._filters:
            raise DiamondRPCFailure('No filters configured')

    @RPCHandlers.handler(25, protocol.XDR_setup, protocol.XDR_blob_list)
    @running(False)
    def setup(self, params):
        self.blast_start = False
        _log.info('Set up called {}'.format(params))
        def log_header(desc):
            _log.info('  %s:', desc)

        def log_item(key, fmt, *args):
            _log.info('    %-14s ' + fmt, key + ':', *args)

        #Scope Cookie Parameters
        try:
            cookies = [ScopeCookie.parse(c) for c in params.cookies]
            _log.info('Scope cookies:')
            for cookie in cookies:
                log_header(cookie.serial)
                log_item('Servers', '%s', ', '.join(cookie.servers))
                log_item('Scopes', '%s', ', '.join(cookie.scopeurls))
                log_item('Expires', '%s', cookie.expires)
        except ScopeCookieExpired as e:
            _log.warning('%s', e)
            raise DiamondRPCCookieExpired()
        except ScopeError as e:
            _log.warning('Cookie invalid: %s', e)
            raise DiamondRPCFailure()

        self._cookie_map = get_cookie_map(cookies)
        self._filters = params.filters
        self._scope = cookies

        # host -> _DiamondConnection
        self._connections = dict((h, _DiamondConnection(h))
                                 for h in self._cookie_map)
        self._blast = _DiamondBlastSet(self._blast_conn, self._connections.values())

        self.missing_blob_list = set()

        proxy_index = 0

        #Proxy filter arguments [name, max_num_filters, majority_frac, threshold]
        for i, f in enumerate(self._filters):
            if f.name == "PROXY":
                self.proxy_filter = ProxyFilter(f)
                proxy_index = i
            self.missing_blob_list.add(f.code)
            self.missing_blob_list.add(f.blob)

        #Removing proxy filter from filter list
        if self.proxy_filter:
            self._filters.pop(proxy_index)

        for h, c in self._connections.items():
            try:
                c.connect()
            except:
                _log.error("Can't start search on %s. "
                           "May be expired cookie, corrupted filters, "
                           "network failure, service not running, "
                           "no space on disk, etc.?", h)
                raise

        return protocol.XDR_blob_list(self.missing_blob_list)

    def get_features_intial(self):
        print("PROXY STARTED {}".format(self.docker_address))
        port = 5000
        mpredict_url = 'http://{}:{}/mpredict'.format(self.docker_address, port)
        predict_url = 'http://{}:{}/predict'.format(self.docker_address, port)
        retry = Retry(total=5, backoff_factor=0.3, method_whitelist=False)
        adapter = HTTPAdapter(max_retries=retry)
        request_session = requests.Session()
        request_session.mount('http://', adapter)
        request_session.mount('https://', adapter)

        print("Training SVM on Input ZipFile...")
        #Extract images from zipfile
        proxy_blob = ZipFile(BytesIO(self.blob_map[self.proxy_filter.blob]), 'r')
        dir_names, dataset = load_dataset_from_zipfile(proxy_blob)

        #Get feature-vectors
        for label, dir_name in enumerate(dir_names):
            files_dct = dict((str(i), io.BytesIO(data))
                        for i, data in enumerate(dataset[dir_name]))
            payload = {'cache': 'true'}
            response = request_session.post(mpredict_url,
                                            files=files_dct, data=payload)
            assert response.ok
            results = response.json()

            for filename, res in results.items():
                if res['success']:
                    feature = res['feature']

                    if label == 1:
                        self.proxy_filter.addItem(label, filename, feature)
                    else:
                        self.proxy_filter.addItem(label, None, feature)
                else:
                    print("Cannot get DNN features for example {} in label {}: {}"
                            .format(filename, dir_name, res['error']))
        return

    @RPCHandlers.handler(26, protocol.XDR_blob_data)
    @running(False)
    def send_blobs(self, params):
        '''Add blobs to the blob cache.'''
        _log.info('Received %d blobs, %d bytes', len(params.blobs),
                  sum([len(b) for b in params.blobs]))

        #Store filter hashcode in hashmap
        for b in params.blobs:
            assert isinstance(b, (str, bytes))
            b = b if isinstance(b, bytes) else b.encode()
            hashcode = 'sha256:' + sha256(b).hexdigest()
            self.blob_map[hashcode] = b

        if self.proxy_filter:
            self.get_features_intial()
            #Train Ensemble
            print("Started model Training...")
            self.proxy_filter.trainEnsemble()
            new_model = self.proxy_filter.getPrunedModels()
            global proxy_model
            with model_lock:
                proxy_model = new_model
            print("Set new model")

        for h, c in self._connections.items():
            try:
                missing_blob = []
                missing = c.setup(self._cookie_map[h], self._filters)
                for m in missing.uris:
                    missing_blob.append(self.blob_map[m])
                if missing_blob:
                    c.send_blobs(missing_blob)
            except:
                _log.error("Can't start search on")

        return


    @RPCHandlers.handler(28, protocol.XDR_start)
    @running(False)
    def start(self, params):
        '''Start the search.'''
        self.blast_start = False
        try:
            self._check_runnable()
        except RPCError as e:
            _log.warning('Cannot start search: %s', str(e))
            raise
        if params.attrs is not None:
            push_attrs = set(params.attrs)
        else:
            # Encode everything
            push_attrs = None
        _log.info('Push attributes:u%s',
                  ','.join(
                      params.attrs) if params.attrs else '(everything)')
        ProxySearch.running = True
        self._running = True
        search_id = str(uuid.uuid4())
        for h, c in self._connections.items():
            c.run_search(search_id.encode(), push_attrs)

        #Start Blast channel
        self._blast.start()
        self.blast_start = True
        threading.Thread(target=self.dispatch_blast,args=(self._blast_conn,self._blast)).start()
        return

    @RPCHandlers.handler(31, protocol.XDR_retrain)
    def retrain_filter(self, params):

        if not self.proxy_filter or not params.names:
            return
        if not params.features[0]:
            return

        ProxySearch.running = False
        #ReTrainFilters
        self.proxy_filter.addItemList(params)
        self.proxy_filter.trainEnsemble()
        new_model = self.proxy_filter.getPrunedModels()
        global proxy_model
        with model_lock:
            proxy_model = new_model
            print("New model created of Length:{}".format(len(proxy_model)))

        ProxySearch.running = True
        return

    @RPCHandlers.handler(30, protocol.XDR_reexecute,
                         protocol.XDR_attribute_list)
    def reexecute_filters(self, params):
        # Just Pass
        try:
            print(params.object_id)
            print(params.hostname)
            print(self._connections[params.hostname])
            return self._connections[params.hostname].control.reexecute_filters(params)
        except ConnectionFailure:
            self.shutdown()
        except RPCError:
            _log.exception('Reexecution Error')
            self.shutdown()

    @RPCHandlers.handler(29, reply_class=protocol.XDR_search_stats)
    @running(True)
    def request_stats(self):
        def combine_into(dest, src):
            for stat in src:
                dest.setdefault(stat.name, 0)
                dest[stat.name] += stat.value

        def get_stats(input_dict):
            result = []
            for k in input_dict.keys():
                global dropped_count
                global true_dropped
                with stats_lock:
                    if k == "objs_processed":
                        input_dict[k] = input_dict[k] - self.previous_stats[k]
                    if k == "objs_dropped":
                        input_dict[k] = input_dict[k] - self.previous_stats[k] + dropped_count
                    if k == "objs_false_negative":
                        input_dict[k] = input_dict[k] - self.previous_stats[k]
                    result.append(protocol.XDR_stat(k, input_dict[k]))
            return result

        try:
            results = [c.control.request_stats() for c in
                       self._connections.values()]
            self.current_stats = {}
            filter_stats = {}
            for result in results:
                combine_into(self.current_stats, result.stats)
                for f in result.filter_stats:
                    combine_into(filter_stats.setdefault(f.name, {}),
                                 f.stats)
            filter_stats_list = [ protocol.XDR_filter_stats(k,
                                    get_stats(filter_stats[k]))
                                    for k in filter_stats.keys()]
            return(protocol.XDR_search_stats(get_stats(self.current_stats), filter_stats_list))
        except RPCError:
            _log.exception('Statistics request failed')
            self.shutdown()



    @RPCHandlers.handler(18, reply_class=protocol.XDR_session_vars)
    @running(True)
    def session_variables_get(self):
        pass


    @RPCHandlers.handler(19, protocol.XDR_session_vars)
    @running(True)
    def session_variables_set(self, params):
        pass


    @staticmethod
    def dispatch_blast(conn, blast_channel):
        while True:
            try:
                #if not blast_channel.queue_empty():
                conn.dispatch(blast_channel)
            except ConnectionFailure:
                break
            except RPCError:
                _log.exception('Cannot receive blast object')
                break


class _DiamondBlastSet(RPCHandlers):
    """
    Pool a set of _DiamondConnection's and return results from them as one stream.
    """

    def __init__(self, conn, connections):
        RPCHandlers.__init__(self)
        # Connections that have not finished searching
        self._conn = conn
        self._connections = set(connections)
        self.pending_objs = deque()
        self.pending_conns = deque()
        self.pending_reqs = deque()
        self._started = False
        self.running = False
        self.high_conf = protocol.XDR_attribute("_score.string", (str(1)+ '\0').encode())
        self.mid_conf = protocol.XDR_attribute("_score.string", (str(2)+ '\0').encode())
        self.low_conf = protocol.XDR_attribute("_score.string", (str(0)+ '\0').encode())
        self.setup_requests()

    def setup_requests(self):
        for _ in range(len(self._connections)):
            self.pending_reqs.append(1)

    def queue_empty(self):
        return not self.pending_objs

    def add_request(self):
        self.pending_reqs.append(1)

    def decrease_request(self):
        if self.pending_reqs:
            self.pending_reqs.popleft()

    def get_prediction(self, features):
        global proxy_model

        #Assuming num_classes = 2
        #Ideally can find num_classes for the max index of model.classes_
        num_classes = 2

        with model_lock:
            num_models = len(proxy_model)
            num_items = features.shape[0]

            pred_proba_list = np.zeros((num_items, num_classes, num_models))
            for j, m in enumerate(proxy_model):
                pred = m.predict_proba(features)
                pred_proba_list[:,m.classes_,j] = pred
            pred_mean = np.mean(pred_proba_list, axis =-1)
            return pred_mean[:,1]

    def fetch_object(self):
        global proxy_model
        while True:

            while not self.pending_objs:
                pass #wait till present

            obj = self.pending_objs.popleft()

            with model_lock:
                if not proxy_model:
                    return obj

            gt_present = False
            features = None
            for a in obj.attrs:
                if a.name == "feature_vector.json":
                    features = np.array([json.loads(a.value)])
                if a.name == "_gt_label":
                    gt_present = True
            if features is None:
                return obj

            pred = self.get_prediction(features)
            pred_attr = protocol.XDR_attribute("prediction.string", (str(pred)+ '\0').encode())
            obj.attrs.append(pred_attr)
            if pred > 0.85:
                obj.attrs.append(self.high_conf)
            elif pred < 0.4:
                # only send 2% of rejected items
                sample = np.random.uniform()
                if gt_present:
                    print("FN pred:{}".format(pred))
                    #TODO Count this in FN
                if sample > 0.02:
                    continue
                global dropped_count
                with stats_lock:
                    dropped_count += 1
                obj.attrs.append(self.low_conf)
            else:
                obj.attrs.append(self.mid_conf)

            return obj

    @RPCHandlers.handler(2, reply_class=protocol.XDR_object)
    def get_object(self):
        while not ProxySearch.running:
            self.pending_objs.clear()
            self.pending_reqs.clear()
            #self._started = False
            self.setup_requests()
            time.sleep(2)
            continue

        if not self._started:
            self.setup_requests()
            time.sleep(2)
            self._started = True

        self.add_request()
        obj = self.fetch_object()
        self.decrease_request()
        return obj

    def start(self):
        """

        :return: A generator that yields search results from underlying connections.
        """
        self._started = True
        self._try_start()
        self.running = True
        return

    def _try_start(self):
        """A generator yielding search results from
        all underlying DiamondConnection's."""
        if self._started:
            def worker(handler):
                try:
                    while True:
                        if not ProxySearch.running:
                            continue
                        obj = next(handler)
                        self.pending_objs.append(obj)
                except StopIteration:
                    print("Stop request called")
                    pass
                finally:
                    self.pending_conns.popleft()

            for conn in self._connections:
                self.pending_conns.append(1)     # just a token
                threading.Thread(target=worker,
                    args=(self._handle_objects(conn,self.pending_reqs),)).start()

    @staticmethod
    def _handle_objects(conn, req_queue):
        """A generator yielding search results from a DiamondConnection."""
        while True:
            if not ProxySearch.running:
                continue
            try:
                while not req_queue:
                    pass
                dct = conn.get_result()
            except ConnectionFailure:
                break
            except RPCError:
                _log.exception('Cannot receive blast object')
                conn.close()
                break

            if not len(dct.attrs):
                break

            yield dct


class _DiamondConnection(object):
    def __init__(self, address):
        self._finished = False  # No more results
        self._closed = False  # Connection closed
        self.address = address
        self.control = ControlConnection()
        self.blast = BlastConnection()

    def connect(self):
        _log.info("Creating control channel to %s", self.address)
        nonce = self.control.connect(self.address)
        _log.info("Done. Nonce %s", binascii.hexlify(nonce))

        _log.info("Creating blast channel. Nonce %s", binascii.hexlify(nonce))
        self.blast.connect(self.address, nonce)
        _log.info("Done.")

    @staticmethod
    def _blob_uri(blob):
        return 'sha256:' + blob.sha256

    def setup(self, cookies, filters):

        # Send setup request
        request = XDR_setup(
            cookies=[c.encode() for c in cookies],
            filters=[XDR_filter_config(
                name=f.name,
                arguments=f.arguments,
                dependencies=f.dependencies,
                min_score=f.min_score,
                max_score=f.max_score,
                code=f.code,
                blob=f.blob
            ) for f in filters],
        )
        reply = self.control.setup(request)

        return reply

    def send_blobs(self, blobs):
        blob = XDR_blob_data(blobs=blobs)
        self.control.send_blobs(blob)

    def run_search(self, search_id, attrs=None):
        _log.info("Running search %s", search_id)
        request = XDR_start(search_id=search_id, attrs=attrs)
        self.control.start(request)

    def get_result(self):
        """
        :return: a dictionary of received attribute-value pairs of an object.
        Return will an empty dict when search terminates.
        """
        reply = self.blast.get_object()
        return reply

    def evaluate(self, cookies, filters, blob, attrs=None):
        """
        Also known as re-execution.
        :param cookies:
        :param filters:
        :param blob:
        :param attrs:
        :return: A dictionary of the re-executed object's attribute-value pairs.
        """
        self.connect()
        self.setup(cookies, filters)

        # Send reexecute request
        request = XDR_reexecute(object_id=self._blob_uri(blob), attrs=attrs)
        try:
            reply = self.control.reexecute_filters(request)
        except DiamondRPCFCacheMiss:
            # Send object data and retry
            self.control.send_blobs(XDR_blob_data(blobs=[str(blob)]))
            reply = self.control.reexecute_filters(request)

        # Return object attributes
        dct = dict((attr.name, attr.value) for attr in reply.attrs)
        return dct

    def close(self):
        if not self._closed:
            self._closed = True
            self.control.close()
            self.blast.close()

