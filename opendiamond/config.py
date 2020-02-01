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

'''Diamond configuration file parsing.'''

from builtins import object
import logging
import os
import socket

import opendiamond
from opendiamond.protocol import PORT

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
        return iter(self._by_attr.values())

    def has_attr(self, key):
        '''Check for parameter existence by attribute name.'''
        return key in self._by_attr

    def get_key(self, key):
        '''Parameter lookup by config key.'''
        return self._by_key[key]


class DiamondConfig(object):
    '''Container for a set of configuration values.'''

    # We dynamically assign object properties, which confuses pylint
    # pylint: disable=no-member,maybe-no-member,access-member-before-definition
    def __init__(self, path=None, **kwargs):
        '''kwargs are attr=value pairs which should override any values
        parsed from the config file.  Only valid config attributes are
        accepted.'''
        # Calculate path to config file (and containing dir).  Priorities:
        # 1. path argument
        # 2. DIAMOND_CONFIG environment var (points to containing directory)
        # 3. $HOME/.diamond/diamond_config
        path_specified = path is not None
        if not path_specified:
            try:
                path = os.path.join(os.environ['DIAMOND_CONFIG'],
                                    'diamond_config')
            except KeyError:
                try:
                    path = os.path.join(os.environ['HOME'], '.diamond',
                                        'diamond_config')
                except KeyError:
                    raise DiamondConfigError("Couldn't get location of "
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
            # -- diamondd
            # Cache directory expiration
            _Param('blob_cache_days', 'BLOBDAYS', 30),
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
            # Debugger to use with debug_filters
            _Param('debug_command', None, 'valgrind'),
            # Names or signatures of filters to run under a debugger
            _Param('debug_filters', None, []),
            # Number of days of logfiles to keep
            _Param('logdays', 'LOGDAYS', 14),
            # Directory for logfiles
            _Param('logdir', 'LOGDIR', os.path.join(confdir, 'log')),
            # logging log level
            _Param('loglevel', 'LOGLEVEL', logging.INFO),
            # Don't fork when a connection arrives
            _Param('oneshot', None, False),
            # HTTP proxy
            _Param('http_proxy', 'HTTP_PROXY', None),
            # Sentry error logging
            _Param('sentry_dsn', 'SENTRY_DSN', None),
            # Canonical server names
            _Param('serverids', 'SERVERID', []),
            # Worker threads per child process
            _Param('threads', 'THREADS', default_threads),
            # HTTP user agent
            _Param('user_agent', None,
                   'OpenDiamond/%s' % opendiamond.__version__),

            # nvidia-docker
            _Param('nv_gpu', "NV_GPU", ''),

            # security: allow insecure mode bypassing expiration date check and signature verification
            _Param('security_cookie_no_verify', 'SECURITY_COOKIE_NO_VERIFY', 0),
            _Param('diamondd_port', 'PORT', PORT),

            # -- dataretriever
            # Listen host
            _Param('retriever_host', 'DRHOST', '127.0.0.1'),
            # Listen port
            _Param('retriever_port', 'DRPORT', 5873),
            # Enabled data stores
            _Param('retriever_stores', 'DATASTORE', []),
            # Diamond store: root data directory
            _Param('dataroot', 'DATAROOT', '/srv/diamond'),
            # Diamond store: root index directory
            _Param('indexdir', 'INDEXDIR', '/src/diamond/INDEXES'),
            # Flickr store: API key
            _Param('flickr_api_key', 'FLICKR_KEY'),
            # Flickr store: API secret
            _Param('flickr_secret', 'FLICKR_SECRET'),
            # Mirage store: repository path
            _Param('mirage_repository', 'MIRAGE_REPOSITORY'),
            # YFCC100M store: metadata database
            _Param('yfcc100m_db_host', 'YFCC100M_DB_HOST', '127.0.0.1'),
            _Param('yfcc100m_db_dbname', 'YFCC100M_DB_DBNAME', 'dataretriever'),
            _Param('yfcc100m_db_user', 'YFCC100M_DB_USER', 'dataretriever'),
            _Param('yfcc100m_db_password', 'YFCC100M_DB_PASSWORD'),
            _Param('yfcc100m_db_port', 'YFCC100M_DB_PORT', 3306),

        )

        # Set defaults
        for param in params.iter_attrs():
            setattr(self, param.attr, param.default)

        # Read config file
        try:
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
            if path_specified:
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
                    os.mkdir(dir, 0o700)
            except OSError:
                raise DiamondConfigError("Couldn't create directory: " + dir)

        # If no server IDs were specified, fall back on server hostnames
        if not self.serverids:
            names = set()
            hostname = socket.getfqdn()
            try:
                for info in socket.getaddrinfo(hostname, None):
                    try:
                        name = socket.getnameinfo(
                            info[4], socket.NI_NAMEREQD)[0]
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
                pass

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

        # Canonicalize debug options
        self.debug_filters = set(self.debug_filters)
        self.debug_command = self.debug_command.split(None)

        # Set default dataretriever stores
        if not self.retriever_stores:
            self.retriever_stores = ['diamond', 'proxy']

    # pylint: enable=no-member,maybe-no-member,access-member-before-definition
