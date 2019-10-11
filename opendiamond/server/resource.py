#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2017-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from builtins import map
from builtins import str
from builtins import object
import docker
import docker.errors
import logging
import multiprocessing as mp
import os
import subprocess
import shlex
import threading
import uuid
from weakref import WeakSet

from opendiamond.config import DiamondConfig
from opendiamond.helpers import murmur
from future.utils import with_metaclass

_log = logging.getLogger(__name__)

# reduce logging from docker library
logging.getLogger('docker').setLevel(logging.WARN)
logging.getLogger('urllib3').setLevel(logging.WARN)

class ResourceTypeError(Exception):
    """Error unrecognized resource type"""

    def __init__(self, rtype):
        super(ResourceTypeError, self).__init__(
            'Unrecognized resource type %s' % rtype)


class ResourceCreationError(Exception):
    """Error creating resource"""


class ResourceCleanupError(Exception):
    """Error cleaning up resource"""


class ResourceContext(object):
    """A resource context is a "namespace" for special resources.
    Resource can be: docker containers, etc.
    All resource must be created within a ResourceContext.
    When a ResourceContext is cleaned up, all resources therein must be
    cleaned up properly. All creations/modifications of resource therein
    are synchronized with a lock.
    ResourceContext can be shared by multiple processes if initialized
    with shared objects from the multiprocessing module"""

    def __init__(self, name, config, lock=threading.Lock(), catalog=dict()):
        super(ResourceContext, self).__init__()
        assert isinstance(config, DiamondConfig)
        self._name = name
        self._config = config
        self._lock = lock
        self._catalog = catalog  # resource signature -> handle (dict)
        _log.info('Created resource context %s', self._name)

    def ensure_resource(self, rtype, *args, **kargs):
        """Ensure a resource exists in the context. If already exists, return the handle.
        If not, create it and return the URI."""
        sig = _ResourceFactory.get_signature(rtype, *args, **kargs)
        with self._lock:
            if sig not in self._catalog: 
                for h in _ResourceFactory.create(rtype, self._config, *args, **kargs):
                    self._catalog[sig] = h
                    _log.debug("Update resource %s => %s", sig, str(h))
            rv = self._catalog[sig]

        return rv

    def cleanup(self):
        _log.info('Cleaning up resource context %s', self._name)
        with self._lock:
            for (_, h) in list(self._catalog.items()):
                _log.debug('Cleaning up %s', str(h))
                _ResourceFactory.cleanup(h)
            self._catalog.clear()


class _ResourceMeta(type):
    def __init__(cls, name, bases, _):
        if not hasattr(cls, 'registry'):
            cls.registry = dict()
        else:
            cls.registry[cls.type] = cls

        super(_ResourceMeta, cls).__init__(name, bases, dict)


class _ResourceFactory(with_metaclass(_ResourceMeta, object)):
    @staticmethod
    def get_signature(rtype, *args, **kargs):
        return murmur(
            rtype 
            + ''.join(map(str, args))
            + ''.join(map(str, list(kargs.keys()))) 
            + ''.join(map(str, list(kargs.values()))))

    @staticmethod
    def create(rtype, config, *args, **kargs):
        try:
            resource_cls = _ResourceFactory.registry[rtype]
            for h in resource_cls.create(config, *args, **kargs):
                # add resource type info in handle
                h['_rtype'] = rtype
                yield h
        except KeyError:
            raise ResourceTypeError(rtype)
        except ResourceCreationError:
            raise

    @staticmethod
    def cleanup(handle):
        rtype = handle.pop('_rtype')
        res_cls = _ResourceFactory.registry[rtype]
        res_cls.cleanup(handle)


class _Docker(_ResourceFactory):
    type = 'docker'

    @staticmethod
    def create(config, image, command, **kargs):
        image = image.strip()
        # Guard against "pull all tags" by the pull() API (if no tag is given,
        # it will pull all tags of that Docker image)
        if not (':' in image or '@sha256' in image):
            image += ":latest"

        name = 'diamond-resource-' + str(uuid.uuid4())
        yield dict(name=name)

        container = None

        try:
            client = docker.from_env()
            client.images.pull(image)
        except docker.errors.APIError:
            raise ResourceCreationError(
                'Unable to pull Docker image %s' % image)
        else:
            try:
                # The immediately returned container object
                # doesn't have sufficient network information.
                container = client.containers.run(
                    image=image,
                    command=shlex.split(command),
                    detach=True,
                    name=name,
                    **kargs
                )
                # Reload until we get the IPAddress
                if container.attrs['HostConfig']['NetworkMode'] == 'default':
                    while not container.attrs['NetworkSettings']['IPAddress']:
                        container.reload()
            except docker.errors.ImageNotFound:
                raise ResourceCreationError(
                    'Docker image not found %s' % image)
            except docker.errors.APIError:
                if container:
                    container.remove(force=True)
                raise ResourceCreationError(
                    'Unable to run Docker container image=%s, command=%s' %
                    (image, command)
                )
            else:
                _log.info(
                    'Started container: (%s, %s), %s, name: %s, IPAddress: %s',
                    image, command, str(kargs), name, container.attrs['NetworkSettings']['IPAddress'])
                yield dict(name=container.name, IPAddress=container.attrs['NetworkSettings']['IPAddress'])

    @staticmethod
    def cleanup(handle):
        try:
            client = docker.from_env()
            container = client.containers.get(handle['name'])
            container.remove(force=True)
        except docker.errors.NotFound:
            _log.warning('Lost container: %s', handle['name'])
        except docker.errors.APIError:
            _log.warning('Unable to remove container %s', container.name)
        else:
            _log.info('Removed container %s', container.name)


class _NvidiaDocker(_Docker):
    type = 'nvidia-docker'

    @staticmethod
    def create(config, image, command):
        image = image.strip()
        # Guard against "pull all tags" by the pull() API
        if not (':' in image or '@sha256' in image):
            image += ":latest"

        name = 'diamond-resource-nvidia-' + str(uuid.uuid4())
        yield dict(name=name)

        container = None

        try:
            client = docker.from_env()
            client.images.pull(image)
        except docker.errors.APIError:
            raise ResourceCreationError(
                'Unable to pull Docker image %s' % image)
        else:
            cmd_l = ['nvidia-docker', 'run', '--detach', '--name', name,
                     image] + \
                    shlex.split(command)

            try:
                my_env = os.environ.copy()
                my_env['NV_GPU'] = config.nv_gpu
                subprocess.check_call(cmd_l, env=my_env)
            except (subprocess.CalledProcessError, OSError):
                try:
                    # In case error happens after the container is created
                    # (e.g., unable to exec command inside)
                    # Remove it at best effort
                    client.containers.get(name).remove(force=True)
                except:  # pylint: disable=bare-except
                    pass
                raise ResourceCreationError(
                    'nvidia-docker unable to start: %s, NV_GPU=%s' % (
                    cmd_l, config.nv_gpu)
                )

            else:
                # Retrieve and bind the container object
                while True:
                    try:
                        container = client.containers.get(name)
                        break
                    except docker.errors.NotFound:
                        pass

                while not container.attrs['NetworkSettings']['IPAddress']:
                    container.reload()

                _log.info(
                    'Started NVIDIA container: (%s, %s), name: %s, IPAddress: %s, NV_GPU: %s',
                    image, command, name, container.attrs['NetworkSettings']['IPAddress'],
                    config.nv_gpu)

                yield dict(name=container.name, IPAddress=container.attrs['NetworkSettings']['IPAddress'])

    @staticmethod
    def cleanup(handle):
        _Docker.cleanup(handle)
