#
#  The OpenDiamond Platform for Interactive Search
#  Version 5
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
import os
import PIL.Image
import struct
import sys
from tempfile import NamedTemporaryFile
import threading

from parameters import Parameters

class Session(object):
    '''Represents the Diamond search session.'''
    def __init__(self, filter_name, conn = None):
        self.name = filter_name
        self.conn = conn

    def log(self, level, message):
        if level == 'critical':
            lval = 0x01
        elif level == 'error':
            lval = 0x02
        elif level == 'info':
            lval = 0x04
        elif level == 'trace':
            lval = 0x08
        elif level == 'debug':
            lval = 0x10
        msg = '{} : {}'.format(self.name, message)
        if self.conn is not None:
            self.conn.send_message('log', lval, msg)
        else:
            # Fallback logging to stderr so that filters can be tested
            # outside of Diamond
            print >>sys.stderr, '[{}] {}'.format(level, msg)

    def get_vars(self, vars):
        '''vars is a tuple of session variables to be atomically read.
        Returns a dict.'''
        if self.conn is None:
            raise RuntimeError('No connection to Diamond')
        self.conn.send_message('get-session-variables', vars)
        return dict(zip(vars, map(float, self.conn.get_array())))

    def update_vars(self, vars):
        '''vars is a dict of session variables to be atomically updated.'''
        if self.conn is None:
            raise RuntimeError('No connection to Diamond')
        names, values = zip(*vars.items())
        self.conn.send_message('update-session-variables', names, values)

class Filter(object):
    '''A Diamond filter.  Implement this.'''
    # Filter name.
    name = 'UNCONFIGURED'
    # Filter instance name.
    instance_name = 'filter'
    # Whether the instance name should be editable in the UI.  Should be
    # True unless the filter bundle is being generated, pre-configured, by
    # some external tool.
    instance_name_editable = True
    # Default drop threshold.
    threshold = 1
    # Whether the threshold should be editable in the UI.
    threshold_editable = False
    # Description of formal parameters accepted by the filter.  These become
    # settings in the HyperFind UI.
    params = Parameters()
    # Filter dependencies
    dependencies = ('RGB',)
    # Set to True if the blob argument is a Python egg that should be added
    # to sys.path.
    blob_is_egg = False

    def __init__(self, args, blob, session = Session('filter')):
        '''Called to initialize the filter.  After a subclass calls the
        superclass constructor, it will find the parsed arguments in
        self.args and the blob, if any, in self.blob (unless self.blob_is_egg
        is True).'''
        self.session = session
        self.args = self.params.parse(args)
        if self.blob_is_egg:
            egg = NamedTemporaryFile(prefix = 'filter-', suffix = '.egg',
                                        delete = False)
            egg.write(blob)
            egg.close()
            sys.path.append(egg.name)
            self.blob = None
        else:
            self.blob = blob

    def __call__(self, object):
        '''Called once for each object to be evaluated.  Returns the Diamond
        search score.'''
        raise NotImplemented()

    @classmethod
    def get_manifest(cls):
        manifest = cls.params.describe()
        manifest.update({
            'Filter': cls.name,
            'Instance': cls.instance_name,
            'Threshold': cls.threshold,
        })
        if len(cls.dependencies) > 0:
            manifest['Dependencies'] = ','.join(cls.dependencies)
        if not cls.instance_name_editable:
            manifest['Instance-Editable'] = 'false'
        if cls.threshold_editable:
            manifest['Threshold-Editable'] = 'true'
        return ''.join(map(lambda kv: '{}: {}\n'.format(kv[0], kv[1]),
                            sorted(manifest.items())))

class LingeringObjectError(Exception):
    '''Raised when an Object is accessed after it is no longer in play.'''
    pass

class Object(object):
    '''A Diamond object to be evaluated.  Acts roughly like a dict of object
    attributes, except that it can't be enumerated, keys can't be deleted,
    and it doesn't provide any of the higher-level dict functions (because
    they're generally not relevant).  Instantiating this class directly will
    provide a dummy object that does not try to talk to Diamond.  This can
    be useful for filter testing.'''

    def __init__(self, attrs = ()):
        self.attrs = dict(attrs)
        self.valid = True

    def __getitem__(self, key):
        self.check_valid()
        if key not in self.attrs:
            self.attrs[key] = self._get_attribute(key)
        if self.attrs[key] is None:
            raise KeyError()
        return self.attrs[key]

    def __setitem__(self, key, value):
        self.check_valid()
        if value is None:
            raise ValueError('Attribute value cannot be None')
        self._set_attribute(key, value)
        self.attrs[key] = value

    def __contains__(self, key):
        self.check_valid()
        try:
            self[key]
        except KeyError:
            return False
        return True

    @property
    def data(self):
        '''Convenience property to get the object data.'''
        return self['']

    @property
    def image(self):
        '''Convenience property to get the decoded RGB image as a PIL Image.'''
        if not hasattr(self, '_image'):
            data = self['_rgb_image.rgbimage']
            # Parse the dimensions out of the RGBImage header
            height, width = struct.unpack('2i', data[8:16])
            # Read the image data
            self._image = PIL.Image.fromstring('RGB', (width, height),
                                    data[16:], 'raw', 'RGBX', 0, 1)
        return self._image

    def omit(self, key):
        '''Tell Diamond not to send the specified attribute back to the
        client by default.  Raises KeyError if the attribute does not exist.'''
        self.check_valid()
        self._omit_attribute(key)

    def check_valid(self):
        if not self.valid:
            raise LingeringObjectError()

    def invalidate(self):
        '''Ensure the Object can't be used to send commands to Diamond once
        Diamond has moved on to another object'''
        self.valid = False

    def _get_attribute(self, key):
        return None

    def _set_attribute(self, key, value):
        pass

    def _omit_attribute(self, key):
        if key not in self.attrs:
            raise KeyError()

class DiamondObject(Object):
    '''A Diamond object to be evaluated.'''

    def __init__(self, conn):
        Object.__init__(self)
        self.conn = conn

    def _get_attribute(self, key):
        self.conn.send_message('get-attribute', key)
        return self.conn.get_item()

    def _set_attribute(self, key, value):
        self.conn.send_message('set-attribute', key, value)

    def _omit_attribute(self, key):
        self.conn.send_message('omit-attribute', key)
        if not self.conn.get_boolean():
            raise KeyError()

class DiamondConnection(object):
    '''Proxy object for the stdin/stdout protocol connection with the
    Diamond server.'''
    def __init__(self, fin, fout):
        self.fin = fin
        self.fout = fout
        self.output_lock = threading.Lock()

    def get_item(self):
        '''Read and return a string or blob.'''
        sizebuf = self.fin.readline()
        if len(sizebuf) == 0:
            # End of file
            raise IOError('End of input stream')
        elif len(sizebuf.strip()) == 0:
            # No length value == no data
            return None
        size = int(sizebuf, 10)
        item = self.fin.read(size)
        if len(item) != size:
            raise IOError('Short read from stream')
        # Swallow trailing newline
        self.fin.read(1)
        return item

    def get_array(self):
        '''Read and return an array of strings or blobs.'''
        arr = []
        while True:
            str = self.get_item()
            if str is None:
                return arr
            arr.append(str)

    def get_boolean(self):
        return self.get_item() == 'true'

    def send_message(self, tag, *values):
        '''Atomically sends a message, consisting of a tag followed by one
        or more values.  An argument can be a list or tuple, in which case
        it is serialized as an array of values terminated by a blank line.'''
        def send_value(value):
            value = str(value)
            self.fout.write('{}\n{}\n'.format(len(value), value))
        with self.output_lock:
            self.fout.write('{}\n'.format(tag))
            for value in values:
                if isinstance(value, list) or isinstance(value, tuple):
                    for element in value:
                        send_value(element)
                    self.fout.write('\n')
                else:
                    send_value(value)
            self.fout.flush()

class StdoutThread(threading.Thread):
    name = 'stdout thread'
    daemon = True

    def __init__(self, stdout_pipe, conn):
        threading.Thread.__init__(self)
        self.pipe = stdout_pipe
        self.conn = conn

    def run(self):
        try:
            while True:
                buf = self.pipe.read(32768)
                if len(buf) == 0:
                    break
                self.conn.send_message('stdout', buf)
        except IOError:
            pass


def run_filter_loop(filter_class):
    try:
        # Set aside stdin and stdout to prevent them from being accessed by
        # mistake, even in forked children
        fin = os.fdopen(os.dup(sys.stdin.fileno()), 'rb', 1)
        fout = os.fdopen(os.dup(sys.stdout.fileno()), 'wb', 32768)
        fh = open('/dev/null', 'r')
        os.dup2(fh.fileno(), 0)
        sys.stdin = os.fdopen(0, 'r')
        fh.close()
        read_fd, write_fd = os.pipe()
        os.dup2(write_fd, 1)
        sys.stdout = os.fdopen(1, 'w', 0)
        os.close(write_fd)
        conn = DiamondConnection(fin, fout)
        # Send the fake stdout to Diamond in the background
        StdoutThread(os.fdopen(read_fd, 'r', 0), conn).start()

        # Read arguments and initialize filter
        ver = int(conn.get_item(), 10)
        if ver != 1:
            raise ValueError('Unknown protocol version {}'.format(ver))
        name = conn.get_item()
        args = conn.get_array()
        blob = conn.get_item()
        session = Session(name, conn)
        filter = filter_class(args, blob, session)
        conn.send_message('init-success')

        # Main loop
        while True:
            obj = DiamondObject(conn)
            conn.send_message('result', filter(obj))
            obj.invalidate()
    except IOError:
        pass

def run_filter(argv, filter_class):
    '''Returns True if we did something, False if not.'''
    if '--filter' in argv:
        run_filter_loop(filter_class)
        return True
    elif '--get-manifest' in argv:
        print filter_class.get_manifest(),
        return True
    else:
        return False
