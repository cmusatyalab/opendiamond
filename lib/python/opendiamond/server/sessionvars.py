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

from __future__ import with_statement
import threading

class _SessionVariable(object):
    def __init__(self):
        self._global_val = 0.0
        self._local_val = 0.0
        self._between_get_and_set_val = 0.0

    def filter_get(self):
        return (self._global_val + self._local_val +
                            self._between_get_and_set_val)

    def filter_update(self, value, between_get_and_set):
        if between_get_and_set:
            # The client has gotten local, but not set global
            self._between_get_and_set_val += value
        else:
            # We are in sync with other servers, update local
            self._local_val += value

    def client_get(self):
        return self._local_val

    def client_set(self, value):
        # Set global value to new value from client
        # (which contains our current local value)
        self._global_val = value

        # Now reset our local_val to be the changes we accumulated
        # while the client was busy contacting other servers
        self._local_val = self._between_get_and_set_val

        # Clear "between" accumulator
        self._between_get_and_set_val = 0.0


class SessionVariables(object):
    def __init__(self):
        self._vars = dict()
        self._lock = threading.Lock()
        self._between_get_and_set = False

    def filter_get(self, keys):
        '''@keys is a list of keys to fetch atomically.  Returns a dict.'''
        ret = dict()
        with self._lock:
            for key in keys:
                if key in self._vars:
                    ret[key] = self._vars[key].filter_get()
                else:
                    ret[key] = 0.0
        return ret

    def filter_update(self, values):
        '''@values is a map of keys and the quantities to add to the
        corresponding values.'''
        with self._lock:
            for key, value in values.iteritems():
                var = self._vars.setdefault(key, _SessionVariable())
                var.filter_update(value, self._between_get_and_set)

    def client_get(self):
        '''Atomically engage the between_get_and_set interlock and return
        a dict of all values.'''
        ret = dict()
        with self._lock:
            self._between_get_and_set = True
            for key, var in self._vars.iteritems():
                ret[key] = var.client_get()
        return ret

    def client_set(self, values):
        '''Atomically release the between_get_and_set interlock and update
        the session variables from the other servers.'''
        with self._lock:
            self._between_get_and_set = False
            for key, value in values.iteritems():
                var = self._vars.setdefault(key, _SessionVariable())
                var.client_set(value)
