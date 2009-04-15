#
#  The OpenDiamond Platform for Interactive Search
#  Version 4
#
#  Copyright (c) 2009 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#
#
# Functions to access the Mirage virtual machine repository
#

MIRAGE_REPOSITORY="/var/lib/mirage"

from wsgiref.util import shift_path_info
from urllib import quote
import subprocess
import os
import re
import struct

__all__ = ['scope_app', 'object_app']


# this expression only matches files because the mode has to start with '-'
mglv_re = re.compile(r"""
    (?P<mode>-.{9})\s+
    (?P<nlink>\d+)\s+
    \d+\s+\d+\s+
    (?P<size>\d+)\s
    (?P<mtime>.{16})\s
    (?P<sha1sum>[a-fA-F0-9]{40})\s
    (?P<path>.+)$""", re.X)

def MirageListVerboseParser(image_id):
    p = subprocess.Popen(['sudo', 'mg', 'list-verbose', '-R',
			  "file://" + MIRAGE_REPOSITORY, image_id],
			 stdout=subprocess.PIPE, close_fds=True)
    yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
    yield '<objectlist>\n'
    for line in p.stdout:
	m = mglv_re.match(line)
	if not m: continue

	res = m.groupdict()
	if res['size'] == '0': # skip empty objects
	    continue

	attrs = '<attr name="content-id">%s</attr>' % image_id
	for key, value in res.items():
	    attrs = attrs + '<attr name="%s">%s</attr>' % (key, quote(value))

	yield '<count adjust="1"><object src="obj/%s">%s</object>\n' % \
	    (res['sha1sum'], attrs)
    yield '</objectlist>'
    p.stdout.close()


# Create file-like wrapper around objects in the Mirage content store
class MirageParseError(Exception):
    pass

class MirageObject:
    def __init__(self, sha1sum):
	# Map from sha1 to content store
	link = os.path.join(MIRAGE_REPOSITORY, "slhome",
			    "com.ibm.mirage.sha1id", sha1sum[:2], sha1sum)
	idx, offset, length = map(int, os.readlink(link).split())

	# Set up wrapper around the object in the content store
	path = os.path.join(MIRAGE_REPOSITORY,"contentfiles","contents.%d"%idx)
	self.f = open(path, 'rb')
	self.f.seek(offset)
	self.length = length

	# Parse out the header
	chunk = self.f.read(27)
	if chunk[:23] != 'com.ibm.mirage.content\0':
	    raise MirageParseError('header mismatch')
	length = struct.unpack('>I', chunk[23:])[0]

	chunk = self.f.read(length + 8)
	if chunk[:length] != 'com.ibm.mirage.sha1id\0':
	    raise MirageParseError('object identifier is not sha1id')
	length = struct.unpack('>Q', chunk[length:])[0]

	chunk = self.f.read(length + 8)
	if chunk[:length-1] != sha1sum:
	    raise MirageParseError('sha1 checksum mismatch')
	length = struct.unpack('>Q', chunk[length:])[0]
	if length != self.length:
	    raise MirageParseError('length mismatch')

    def close(self):
	self.f.close()

    def read(self, size=4096):
	if size > self.length:
	    size = self.length
	chunk = self.f.read(size)
	self.length = self.length - len(chunk)
	return chunk


def scope_app(environ, start_response):
    root = shift_path_info(environ)
    if root == 'obj':
	return object_app(environ, start_response)

    image_id = 'com.ibm.mirage.sha1id/' + root.lower()

    start_response("200 OK", [('Content-Type', "text/xml")])
    return MirageListVerboseParser(image_id)


def object_app(environ, start_response):
    sha1sum = environ['PATH_INFO'].lower()
    f = MirageObject(sha1sum)

    headers = [('Content-Length', str(f.length)), ('ETag', sha1sum)]

    if_none = environ.get('HTTP_IF_NONE_MATCH')
    if if_none and (if_none == '*' or etag in if_none):
	start_response("304 Not Modified", headers)
	return [""]

    start_response("200 OK", headers)
    # wrap the file object in an iterator that reads the file in 64KB
    # blocks instead of line-by-line.
    return environ['wsgi.file_wrapper'](f, 65536)

