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

'''Diamond configuration file parsing.'''

import os
import socket

class DiamondConfigError(Exception):
    pass


class _Param(object):
    '''Descriptor for a single configuration parameter.'''

    def __init__(self, attr=None, config_key=None, default=None):
        '''attr is None for deprecated config keys.  config_key is None for
        parameters not available via the config file.  default is None when
        it actually is.'''
        self.attr = attr
        self.config_key = config_key
        self.default = default


class _ConfigParams(object):
    '''The set of valid configuration parameters.'''

    def __init__(self, *args):
        self._by_attr = dict()
        self._by_key = dict()
        for param in args:
            if param.attr is not None:
                self._by_attr[param.attr] = param
            if param.config_key is not None:
                self._by_key[param.config_key] = param

    def iter_attrs(self):
        '''Return iterator over _Params with defined attribute names.'''
        return self._by_attr.itervalues()

    def has_attr(self, key):
        '''Check for parameter existence by attribute name.'''
        return key in self._by_attr

    def get_key(self, key):
        '''Parameter lookup by config key.'''
        return self._by_key[key]


class DiamondConfig(object):
    '''Container for a set of configuration values.'''

    def __init__(self, path=None, **kwargs):
        '''kwargs are attr=value pairs which should override any values
        parsed from the config file.  Only valid config attributes are
        accepted.'''
        # Calculate path to config file (and containing dir).  Priorities:
        # 1. path argument
        # 2. DIAMOND_CONFIG environment var (points to containing directory)
        # 3. $HOME/.diamond/diamond_config
        if path is None:
            try:
                path = os.path.join(os.environ['DIAMOND_CONFIG'],
                                    'diamond_config')
            except KeyError:
                try:
                    path = os.path.join(os.environ['HOME'], '.diamond',
                                        'diamond_config')
                except KeyError:
                    raise DiamondConfigError("Couldn't get location of " +
                                        "diamond_config")
        confdir = os.path.dirname(path)

        # Determine the default number of diamondd worker threads.  On
        # Linux, the sysconf variable counts each hyperthread context as a
        # separate CPU, so this value may be too high.  In order to ensure
        # adequate testing of the multi-threaded case, never default to
        # fewer than two threads.
        try:
            default_threads = max(os.sysconf('SC_NPROCESSORS_ONLN'), 2)
        except OSError:
            default_threads = 2

        # Define configuration parameters
        params = _ConfigParams(
            ## diamondd
            # Redis database
            _Param('cache_database', 'CACHEDB', 0),
            # Redis password
            _Param('cache_password', 'CACHEPASSWD', None),
            # Redis host and port
            _Param('cache_server', 'CACHE', None),
            # Cache directory
            _Param('cachedir', 'CACHEDIR', os.path.join(confdir, 'cache')),
            # PEM data for scope cookie signing certificates
            _Param('certdata', None, ''),
            # File containing PEM-encoded scope cookie signing certificates;
            # will be loaded into certdata
            _Param('certfile', 'CERTFILE', os.path.join(confdir, 'CERTS')),
            # Root directory of control group filesystem
            _Param('cgroupdir', 'CGROUPDIR'),
            # Fork to background
            _Param('daemonize', None, True),
            # Bind listening socket only to localhost
            _Param('localhost_only', None, False),
            # Directory for logfiles
            _Param('logdir', 'LOGDIR', os.path.join(confdir, 'log')),
            # Don't fork when a connection arrives
            _Param('oneshot', None, False),
            # Canonical server names
            _Param('serverids', 'SERVERID', []),
            # Worker threads per child process
            _Param('threads', 'THREADS', default_threads),

            ## dataretriever Diamond store
            # Root data directory
            _Param('dataroot', 'DATAROOT'),
            # Root index directory
            _Param('indexdir', 'INDEXDIR'),

            ## Deprecated config keys
            _Param(None, 'DATATYPE'),
        )

        # Set defaults
        for param in params.iter_attrs():
            setattr(self, param.attr, param.default)

        # Read config file
        try:
            config = dict()
            for line in open(path):
                line = line.strip()
                if line == '' or line[0] == '#':
                    continue
                try:
                    key, value = line.split(None, 1)
                    try:
                        param = params.get_key(key)
                    except KeyError:
                        raise DiamondConfigError('Unknown config key: ' + key)
                    if param.attr is not None:
                        if isinstance(param.default, list):
                            getattr(self, param.attr).append(value)
                        elif isinstance(param.default, int):
                            setattr(self, param.attr, int(value))
                        else:
                            setattr(self, param.attr, value)
                except ValueError:
                    raise DiamondConfigError("Syntax error: %s" % line.strip())
        except IOError:
            raise DiamondConfigError("Couldn't read %s" % path)

        # Process overrides in keyword arguments
        for attr in kwargs:
            if params.has_attr(attr):
                setattr(self, attr, kwargs[attr])
            else:
                raise AttributeError('Could not override invalid ' +
                                    'attribute ' + attr)

        # Create directories
        for dir in self.cachedir, self.logdir:
            try:
                if dir is not None and not os.path.isdir(dir):
                    os.mkdir(dir, 0700)
            except OSError:
                raise DiamondConfigError("Couldn't create directory: " + dir)

        # If no server IDs were specified, fall back on server hostnames
        if len(self.serverids) == 0:
            names = set()
            hostname = socket.getfqdn()
            try:
                for info in socket.getaddrinfo(hostname, None):
                    try:
                        name = socket.getnameinfo(info[4],
                                        socket.NI_NAMEREQD)[0]
                        names.add(name)
                    except socket.gaierror:
                        pass
            except socket.gaierror:
                pass
            self.serverids = list(names)

        # Load certificate file if specified and not overridden
        if self.certfile is not None and self.certdata == '':
            try:
                self.certdata = open(self.certfile).read()
            except IOError:
                raise DiamondConfigError("Couldn't read certificate file: "
                                + self.certfile)

        # Parse the Redis server address if specified
        if self.cache_server is not None:
            if ':' not in self.cache_server:
                self.cache_server += ':6379'
            host, port = self.cache_server.split(':', 1)
            try:
                port = int(port)
            except ValueError:
                raise DiamondConfigError('Invalid port number: ' + port)
            self.cache_server = (host, port)
