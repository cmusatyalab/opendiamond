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
from xml.dom import minidom
from xml.etree.ElementTree import ElementTree

from opendiamond.bundle import element
from opendiamond.filter.options import OptionList

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


class Search(object):
    '''A search, comprising zero or more configuration options and one or
    more filters.'''

    def __init__(self, display_name, filters, options=None):
        '''filters is a list of filter classes (not instances).  options
        is a list of option instances.'''
        if not filters:
            raise ValueError("At least one filter must be specified")
        if options is None:
            options = []
        self.display_name = display_name
        self._filters = _FilterList(filters)
        self._options = OptionList(options)

        # Check all filters for valid references
        option_names = set(self._options.get_names())
        filter_labels = set(f.label for f in filters if f.label is not None)
        self._filters.check_config(option_names, filter_labels)

    def run(self, argv=sys.argv):
        '''Returns True if we did something, False if not.'''
        if '--filter' in argv:
            self._run_loop()
            return True
        elif '--get-manifest' in argv:
            print self.get_manifest(),
            return True
        else:
            return False

    def get_manifest(self):
        '''Return an XML document describing this search.'''
        root = element('search', {
            'xmlns': 'http://diamond.cs.cmu.edu/xmlns/opendiamond/bundle-1',
            'displayName': self.display_name,
        })
        opts = self._options.describe()
        if len(opts) > 0:
            root.append(opts)
        root.append(self._filters.describe())
        buf = StringIO()
        etree = ElementTree(root)
        etree.write(buf, encoding='UTF-8', xml_declaration=True)
        # Now run the data through minidom for pretty-printing
        dom = minidom.parseString(buf.getvalue())
        return dom.toprettyxml(indent='  ', encoding='UTF-8')

    def _run_loop(self):
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
            filter = self._filters.get_filter(self._options, args, blob,
                                    session)
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


class Filter(object):
    '''A Diamond filter.  Implement this.'''
    # Fixed name for this filter on the wire.  This should not be used
    # unless there is a specific reason to hardcode the filter name.
    fixed_name = None
    # Filter label for dependency references.
    label = None
    # If not None, the minimum filter score to accept.  If a Ref, the name
    # of an option specifying the minimum filter score.
    min_score = 1
    # If not None, the maximum filter score to accept.  If a Ref, the name
    # of an option specifying the maximum filter score.
    max_score = None
    # Filter dependencies.  Strings represent fixed filter names; Ref
    # objects represent filter labels.
    dependencies = ('RGB',)
    # Names of options to be passed as arguments to this filter.
    arguments = ()
    # Name of the file (within the bundle or in the filesystem) to use as
    # the blob argument.  If a Ref, the name of an option specifying the
    # filename.
    blob = None
    # Set to True if the blob argument is a Python egg that should be added
    # to sys.path.
    blob_is_egg = False

    def __init__(self, args, blob, session=Session('filter')):
        '''Called to initialize the filter.  After a subclass calls the
        constructor, it will find the parsed arguments stored as object
        attributes named after the options, and the blob, if any, in self.blob
        (unless self.blob_is_egg is True).'''
        for k, v in args.iteritems():
            setattr(self, k, v)
        if self.blob_is_egg:
            # NamedTemporaryFile always deletes the file on close on
            # Python 2.5, so we can't use it
            fd, name = mkstemp(prefix='filter-', suffix='.egg')
            egg = os.fdopen(fd, 'r+')
            egg.write(blob)
            egg.close()
            sys.path.append(name)
            self.blob = None
        else:
            self.blob = blob
        self.session = session

    def __call__(self, object):
        '''Called once for each object to be evaluated.  Returns the Diamond
        search score.'''
        raise NotImplementedError()

    @classmethod
    def check_config(cls, option_names, filter_labels):
        '''Ensure all option and filter references are valid.'''
        for dep in cls.dependencies:
            if isinstance(dep, Ref) and str(dep) not in filter_labels:
                raise ValueError('Unknown filter label: %s' % dep)
        for arg in cls.arguments:
            if str(arg) not in option_names:
                raise ValueError('Unknown option name: %s' % arg)
        for val in cls.min_score, cls.max_score:
            if isinstance(val, Ref) and str(val) not in option_names:
                raise ValueError('Unknown option name: %s' % val)

    @classmethod
    def describe(cls, filter_index):
        el = element('filter', {
            'fixedName': cls.fixed_name,
            'label': cls.label,
            'code': 'filter',
        })
        # Filter thresholds
        for name, value in (('minScore', cls.min_score),
                            ('maxScore', cls.max_score)):
            if isinstance(value, Ref):
                el.append(element(name, {'option': str(value)}))
            elif value is not None:
                el.append(element(name, {'value': value}))
        # Filter dependencies
        if cls.dependencies:
            dependencies = element('dependencies')
            for dep in cls.dependencies:
                if isinstance(dep, Ref):
                    attrs = {'label': str(dep)}
                else:
                    attrs = {'fixedName': dep}
                dependencies.append(element('dependency', attrs))
            el.append(dependencies)
        # Filter arguments.  Always add an initial argument specifying
        # which Filter to execute.
        arguments = element('arguments')
        arguments.append(element('argument', {'value': filter_index}))
        for opt in cls.arguments:
            arguments.append(element('argument', {'option': opt}))
        el.append(arguments)
        # Blob argument
        if cls.blob is not None:
            if isinstance(cls.blob, Ref):
                attrs = {'option': str(cls.blob)}
            else:
                attrs = {'data': cls.blob}
            el.append(element('blob', attrs))
        return el


class Ref(object):
    '''A reference to a filter label or option name.'''
    def __init__(self, val):
        self._val = val

    def __str__(self):
        return self._val


class _FilterList(object):
    '''A list of filter classes in a search.'''

    def __init__(self, filters):
        '''filters is a list of filter classes (not instances).'''
        self._filters = tuple(filters)

    def check_config(self, option_names, filter_labels):
        '''Check all filters for valid references.'''
        for filter in self._filters:
            filter.check_config(option_names, filter_labels)

    def describe(self):
        '''Return an XML element describing the filter list.'''
        filters = element('filters')
        for i, filter in enumerate(self._filters):
            filters.append(filter.describe(i))
        return filters

    def get_filter(self, options, args, blob, session):
        '''Return a Filter instance initialized with the specified argument
        list, blob argument, and session state.'''
        # Determine which Filter to configure
        index = int(args.pop(0))
        filter_class = self._filters[index]
        # Parse arguments and initialize instance
        argmap = options.parse([str(s) for s in filter_class.arguments], args)
        return filter_class(argmap, blob, session)


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
