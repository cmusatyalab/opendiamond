import shlex
import threading

import logging
import uuid

from weakref import WeakSet

import subprocess

from opendiamond.helpers import murmur
import docker
import docker.errors

_log = logging.getLogger(__name__)


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
    are synchronized with a lock."""

    def __init__(self, name):
        super(ResourceContext, self).__init__()
        self._name = name
        self._lock = threading.Lock()
        self._catalog = dict()
        self._subcontexts = WeakSet()
        _log.debug('Created resource context %s', self._name)

    def ensure_resource(self, rtype, *args):
        """Ensure a resource exists in the context. If already exists, return the URI.
        If not, create it and return the URI."""
        sig = _ResourceFactory.get_signature(rtype, *args)
        with self._lock:
            if sig not in self._catalog:
                res = _ResourceFactory.create(rtype, *args)
                self._catalog[sig] = res
            rv = self._catalog[sig].uri

        return rv

    def create_subcontext(self, name):
        sc = ResourceContext(name)
        self._subcontexts.add(sc)
        return sc

    def cleanup(self):
        """Recursively clean up all sub-contexts under this one, including itself.
        This method could be called multiple times."""
        _log.debug('Destroying resource context %s', self._name)
        with self._lock:
            # Clean up descendants
            for sc in self._subcontexts:
                sc.cleanup()
            self._subcontexts.clear()

            # Clean up myself
            for (_, res) in self._catalog.iteritems():
                res.cleanup()
            self._catalog.clear()

    __del__ = cleanup


class _ResourceMeta(type):
    def __init__(cls, name, bases, _):
        if not hasattr(cls, 'registry'):
            cls.registry = dict()
        else:
            cls.registry[cls.type] = cls

        super(_ResourceMeta, cls).__init__(name, bases, dict)


class _ResourceFactory(object):
    __metaclass__ = _ResourceMeta

    @staticmethod
    def get_signature(rtype, *args):
        return murmur(rtype + '' + ''.join(args))

    @staticmethod
    def create(rtype, *args):
        try:
            resource_cls = _ResourceFactory.registry[rtype]
            return resource_cls(*args)
        except KeyError:
            raise ResourceTypeError(rtype)
        except ResourceCreationError:
            raise

    def cleanup(self):
        raise NotImplementedError()

    @property
    def uri(self):
        """A dict to be passed back to filters"""
        raise NotImplementedError()


class _Docker(_ResourceFactory):
    type = 'docker'

    def __init__(self, image, command):
        image = image.strip()
        name = 'diamond-resource-' + str(uuid.uuid4())

        try:
            client = docker.from_env()
            # The run() method will auto pull the image.
            # run() returns immediately. The returned container object
            # doesn't have sufficient network information.
            self._container = client.containers.run(
                image=image,
                command=shlex.split(command),
                detach=True,
                name=name
            )
            # Reload until we get the IPAddress
            while self._container.attrs['NetworkSettings']['IPAddress'] == '':
                self._container.reload()
        except docker.errors.ImageNotFound:
            raise ResourceCreationError('Docker image not found %s' % image)
        except docker.errors.APIError:
            if hasattr(self, '_container'):
                self._container.remove(force=True)
            raise ResourceCreationError(
                'Docker server unable to run image=%s, command=%s' %
                (image, command)
            )
        else:
            _log.info('Started container: (%s, %s), name: %s, IPAddress: %s',
                      image, command, self.uri['name'], self.uri['IPAddress'])

    @property
    def uri(self):
        rv = dict(
            name=self._container.name,
            IPAddress=self._container.attrs['NetworkSettings']['IPAddress']
        )
        return rv

    def cleanup(self):
        try:
            self._container.remove(force=True)
        except docker.errors.APIError:
            _log.warning('Unable to remove container %s', self._container.name)
        else:
            _log.info('Stopped container %s', self._container.name)


class _NvidiaDocker(_Docker):
    type = 'nvidia-docker'

    # pylint: disable=super-init-not-called
    def __init__(self, image, command):
        image = image.strip()
        name = 'diamond-resource-nvidia-' + str(uuid.uuid4())
        cmd_l = ['nvidia-docker', 'run', '--detach', '--name', name, image] + \
            shlex.split(command)

        try:
            # _log.debug('Creating nvidia-docker: %s' % cmd_l)
            subprocess.check_call(cmd_l)
        except (subprocess.CalledProcessError, OSError):
            try:
                # In case error happens after the container is created (e.g.,
                # unable to exec command inside)
                client = docker.from_env()
                client.containers.get(name).remove(force=True)
            except:  # pylint: disable=bare-except
                pass
            raise ResourceCreationError(
                'nvidia-docker unable to start: %s' % cmd_l
            )

        # Retrieve and bind the container object
        client = docker.from_env()
        while True:
            try:
                self._container = client.containers.get(name)
                break
            except docker.errors.NotFound:
                pass

        while self._container.attrs['NetworkSettings']['IPAddress'] == '':
            self._container.reload()
