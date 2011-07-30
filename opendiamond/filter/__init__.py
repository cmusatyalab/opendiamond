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
from cStringIO import StringIO
import os
import PIL.Image
import struct
import sys
from tempfile import mkstemp
import threading
from zipfile import ZipFile

from opendiamond.bundle import BUNDLE_NS, element, format_manifest

class Session(object):
    '''Represents the Diamond search session.'''
    def __init__(self, filter_name, conn=None):
        self.name = filter_name
        self._conn = conn

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
        msg = '%s : %s' % (self.name, message)
        if self._conn is not None:
            self._conn.send_message('log', lval, msg)
        else:
            # Fallback logging to stderr so that filters can be tested
            # outside of Diamond
            print >> sys.stderr, '[%s] %s' % (level, msg)

    def get_vars(self, vars):
        '''vars is a tuple of session variables to be atomically read.
        Returns a dict.'''
        if self._conn is None:
            raise RuntimeError('No connection to Diamond')
        self._conn.send_message('get-session-variables', vars)
        return dict(zip(vars, [float(v) for v in self._conn.get_array()]))

    def update_vars(self, vars):
        '''vars is a dict of session variables to be atomically updated.'''
        if self._conn is None:
            raise RuntimeError('No connection to Diamond')
        names, values = zip(*vars.items())
        self._conn.send_message('update-session-variables', names, values)


class Filter(object):
    '''A Diamond filter.  Implement this.'''
    # List of Parameters representing argument types and the corresponding
    # attributes to store the arguments in.  For example, argument 0 will be
    # stored in a Filter attribute named by params[0].
    params = ()
    # Set to "egg" if the blob argument is a Python egg that should be added
    # to sys.path.  In this case, self.blob is set to None.
    # Set to "zip" if the blob argument is a Zip file.  self.blob will be
    # set to a ZipFile object.
    # Otherwise, self.blob will be set to the blob data.
    blob_format = None

    def __init__(self, args, blob, session=Session('filter')):
        '''Called to initialize the filter.  After a subclass calls the
        constructor, it will find the parsed arguments stored as object
        attributes as specified by the parameters, and the blob, if any,
        in self.blob (unless self.blob_is_egg is True).'''
        if len(args) != len(self.params):
            raise ValueError('Incorrect argument list length')
        for param, arg in zip(self.params, args):
            setattr(self, str(param), param.parse(arg))
        if self.blob_format == 'egg':
            # NamedTemporaryFile always deletes the file on close on
            # Python 2.5, so we can't use it
            fd, name = mkstemp(prefix='filter-', suffix='.egg')
            egg = os.fdopen(fd, 'r+')
            egg.write(blob)
            egg.close()
            sys.path.append(name)
            self.blob = None
        elif self.blob_format == 'zip':
            self.blob = ZipFile(StringIO(blob), 'r')
        else:
            self.blob = blob
        self.session = session

    def __call__(self, object):
        '''Called once for each object to be evaluated.  Returns the Diamond
        search score.'''
        raise NotImplementedError()

    @classmethod
    def run(cls, classes=None, argv=sys.argv):
        '''Try to run the filter.  Returns True if we did something,
        False if not.

        If classes is specified, it is a list of filter classes that
        should be supported.  In this case, the first filter argument must
        specify the name of the class that should be executed during this
        run of the program.  That argument will be stripped from the
        argument list before it is given to the filter.'''
        if '--filter' in argv:
            cls._run_loop(classes)
            return True
        else:
            return False

    @classmethod
    def _run_loop(cls, classes=None):
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
            conn = _DiamondConnection(fin, fout)
            # Send the fake stdout to Diamond in the background
            _StdoutThread(os.fdopen(read_fd, 'r', 0), conn).start()

            # Read arguments and initialize filter
            ver = int(conn.get_item())
            if ver != 1:
                raise ValueError('Unknown protocol version %d' % ver)
            name = conn.get_item()
            args = conn.get_array()
            blob = conn.get_item()
            session = Session(name, conn)
            if classes is not None:
                # Use the class named by the first filter argument
                target = args.pop(0)
                for class_ in classes:
                    if class_.__name__ == target:
                        filter_class = class_
                        break
                else:
                    raise ValueError('Filter class %s is not available' %
                                        target)
            else:
                filter_class = cls
            filter = filter_class(args, blob, session)
            conn.send_message('init-success')

            # Main loop
            while True:
                obj = _DiamondObject(conn)
                result = filter(obj)
                if result is True:
                    result = 1
                elif result is False or result is None:
                    result = 0
                conn.send_message('result', result)
                obj.invalidate()
        except IOError:
            pass


class _DummyFilterImpl(Filter):
    '''Dummy class to silence pylint R0921, which can't be squelched via
    comments.'''
    def __call__(self, object):
        pass
class _DummyFilterImpl2(Filter):
    '''Dummy class to silence pylint R0922, which also can't be squelched via
    comments.'''
    def __call__(self, object):
        pass


class LingeringObjectError(Exception):
    '''Raised when an Object is accessed after it is no longer in play.'''
    pass


class Object(object):
    '''A Diamond object to be evaluated.  Instantiating this class directly
    will provide a dummy object that does not try to talk to Diamond.  This
    can be useful for filter testing.'''

    def __init__(self, attrs=()):
        self._attrs = dict(attrs)
        self._valid = True
        self._image = None

    def get_binary(self, key):
        '''Get the specified object attribute as raw binary data.'''
        self.check_valid()
        if key not in self._attrs:
            self._attrs[key] = self._get_attribute(key)
        if self._attrs[key] is None:
            raise KeyError()
        return self._attrs[key]

    def set_binary(self, key, value):
        '''Set the specified object attribute as raw binary data.'''
        self.check_valid()
        if value is None:
            raise ValueError('Attribute value cannot be None')
        self._set_attribute(key, value)
        self._attrs[key] = value

    def get_string(self, key):
        '''Get the specified object attribute, interpreting the raw data
        as a null-terminated string.'''
        value = self.get_binary(key)
        if value[-1] != '\0':
            raise ValueError('Attribute value is not null-terminated')
        return value[:-1]

    def set_string(self, key, value):
        '''Set the specified object attribute as a null-terminated string.'''
        self.set_binary(key, str(value) + '\0')

    def get_int(self, key):
        '''Get the specified object attribute, interpreting the raw data
        as a native-endian integer.  The key name should end with ".int".'''
        return struct.unpack('i', self.get_binary(key))[0]

    def set_int(self, key, value):
        '''Set the specified object attribute as an integer.  The key name
        should end with ".int".'''
        self.set_binary(key, struct.pack('i', value))

    def get_double(self, key):
        '''Get the specified object attribute, interpreting the raw data
        as a native-endian double.  The key name should end with ".double".'''
        return struct.unpack('d', self.get_binary(key))[0]

    def set_double(self, key, value):
        '''Set the specified object attribute as a double.  The key name
        should end with ".double".'''
        self.set_binary(key, struct.pack('d', value))

    def get_rgbimage(self, key):
        '''Get the specified object attribute, interpreting the raw data
        as an RGBImage structure.  The key name should end with ".rgbimage".'''
        data = self.get_binary(key)
        # Parse the dimensions out of the header
        height, width = struct.unpack('2i', data[8:16])
        # Read the image data
        return PIL.Image.fromstring('RGB', (width, height), data[16:],
                                'raw', 'RGBX', 0, 1)

    def set_rgbimage(self, key, value):
        '''Set the specified object attribute as an RGBImage structure.
        The key name should end with ".rgbimage".'''
        pixels = value.tostring('raw', 'RGBX', 0, 1)
        width, height = value.size
        header = struct.pack('IIii', 0, len(pixels) + 16, height, width)
        self.set_binary(key, header + pixels)

    def get_patches(self, key):
        '''Get the specified object attribute as a list of patches.  Returns
        (distance, patches), where patches is a tuple of (upper_left_coord,
        lower_right_coord) tuples and a coordinate is an (x, y) tuple.'''
        def parse(fmt, data):
            len = struct.calcsize(fmt)
            data, remainder = data[0:len], data[len:]
            return (remainder,) + struct.unpack(fmt, data)
        data = self.get_binary(key)
        data, count, distance = parse('<id', data)
        patches = []
        for _i in range(count):
            data, x0, y0, x1, y1 = parse('<iiii', data)
            patches.append(((x0, y0), (x1, y1)))
        return distance, tuple(patches)

    def set_patches(self, key, distance, patches):
        '''Set the specified object attribute as a list of patches.  distance
        is a double.  patches is a list of (upper_left_coord,
        lower_right_coord) tuples, where a coordinate is an (x, y) tuple.
        The key name should probably be _filter.%s.patches, where %s is the
        filter name from Session.'''
        pieces = [struct.pack('<id', len(patches), distance)]
        for a, b in patches:
            pieces.append(struct.pack('<iiii', a[0], a[1], b[0], b[1]))
        self.set_binary(key, ''.join(pieces))

    def __getitem__(self, key):
        '''Syntactic sugar for self.get_string().'''
        return self.get_string(key)

    def __setitem__(self, key, value):
        '''Syntactic sugar for self.set_string().'''
        return self.set_string(key, value)

    def __contains__(self, key):
        self.check_valid()
        try:
            self.get_binary(key)
        except KeyError:
            return False
        return True

    @property
    def data(self):
        '''Convenience property to get the object data.'''
        return self.get_binary('')

    @property
    def image(self):
        '''Convenience property to get the decoded RGB image as a PIL Image.'''
        if self._image is None:
            self._image = self.get_rgbimage('_rgb_image.rgbimage')
        return self._image

    def omit(self, key):
        '''Tell Diamond not to send the specified attribute back to the
        client by default.  Raises KeyError if the attribute does not exist.'''
        self.check_valid()
        self._omit_attribute(key)

    def check_valid(self):
        if not self._valid:
            raise LingeringObjectError()

    def invalidate(self):
        '''Ensure the Object can't be used to send commands to Diamond once
        Diamond has moved on to another object'''
        self._valid = False

    def _get_attribute(self, _key):
        return None

    def _set_attribute(self, _key, _value):
        pass

    def _omit_attribute(self, key):
        if key not in self._attrs:
            raise KeyError()


class _DiamondObject(Object):
    '''A Diamond object to be evaluated.'''

    def __init__(self, conn):
        Object.__init__(self)
        self._conn = conn

    def _get_attribute(self, key):
        self._conn.send_message('get-attribute', key)
        return self._conn.get_item()

    def _set_attribute(self, key, value):
        self._conn.send_message('set-attribute', key, value)

    def _omit_attribute(self, key):
        self._conn.send_message('omit-attribute', key)
        if not self._conn.get_boolean():
            raise KeyError()


class _DiamondConnection(object):
    '''Proxy object for the stdin/stdout protocol connection with the
    Diamond server.'''
    def __init__(self, fin, fout):
        self._fin = fin
        self._fout = fout
        self._output_lock = threading.Lock()

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

    def get_boolean(self):
        return self.get_item() == 'true'

    def send_message(self, tag, *values):
        '''Atomically sends a message, consisting of a tag followed by one
        or more values.  An argument can be a list or tuple, in which case
        it is serialized as an array of values terminated by a blank line.'''
        def send_value(value):
            value = str(value)
            self._fout.write('%d\n%s\n' % (len(value), value))
        with self._output_lock:
            self._fout.write('%s\n' % tag)
            for value in values:
                if isinstance(value, list) or isinstance(value, tuple):
                    for el in value:
                        send_value(el)
                    self._fout.write('\n')
                else:
                    send_value(value)
            self._fout.flush()


class _StdoutThread(threading.Thread):
    name = 'stdout thread'

    def __init__(self, stdout_pipe, conn):
        threading.Thread.__init__(self)
        self.setDaemon(True)
        self._pipe = stdout_pipe
        self._conn = conn

    def run(self):
        try:
            while True:
                buf = self._pipe.read(32768)
                if len(buf) == 0:
                    break
                self._conn.send_message('stdout', buf)
        except IOError:
            pass
