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

'''Session variables.'''

from __future__ import with_statement
from builtins import object
import threading


class _SessionVariable(object):
    '''A single session variable.'''

    def __init__(self):
        self._global_val = 0.0
        self._local_val = 0.0
        self._between_get_and_set_val = 0.0

    def filter_get(self):
        '''Return the total value of the variable.'''
        return (self._global_val + self._local_val +
                self._between_get_and_set_val)

    def filter_update(self, value, between_get_and_set):
        '''Add a new value produced by a filter into the variable.'''
        if between_get_and_set:
            # The client has gotten local, but not set global
            self._between_get_and_set_val += value
        else:
            # We are in sync with other servers, update local
            self._local_val += value

    def client_get(self):
        '''Return that part of the variable's value not already known to
        the client.'''
        return self._local_val

    def client_set(self, value):
        '''Process an update of the variable sent by the client.'''
        # Set global value to new value from client
        # (which contains our current local value)
        self._global_val = value

        # Now reset our local_val to be the changes we accumulated
        # while the client was busy contacting other servers
        self._local_val = self._between_get_and_set_val

        # Clear "between" accumulator
        self._between_get_and_set_val = 0.0


class SessionVariables(object):
    '''A set of session variables.'''

    def __init__(self):
        self._vars = dict()
        self._lock = threading.Lock()
        self._between_get_and_set = False

    def filter_get(self, keys):
        '''Return a dict giving the total values of the variables listed in
        keys.'''
        ret = dict()
        with self._lock:
            for key in keys:
                if key in self._vars:
                    ret[key] = self._vars[key].filter_get()
                else:
                    ret[key] = 0.0
        return ret

    def filter_update(self, values):
        '''Add new values produced by a filter into the specified variables.
        @values is a map of keys and the quantities to add to the
        corresponding values.'''
        with self._lock:
            for key, value in values.items():
                var = self._vars.setdefault(key, _SessionVariable())
                var.filter_update(value, self._between_get_and_set)

    def client_get(self):
        '''Atomically engage the between_get_and_set interlock and return
        a dict of all values.'''
        ret = dict()
        with self._lock:
            self._between_get_and_set = True
            for key, var in self._vars.items():
                ret[key] = var.client_get()
        return ret

    def client_set(self, values):
        '''Atomically release the between_get_and_set interlock and update
        the session variables from the other servers.'''
        with self._lock:
            self._between_get_and_set = False
            for key, value in values.items():
                var = self._vars.setdefault(key, _SessionVariable())
                var.client_set(value)
