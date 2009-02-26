#!/usr/bin/python

import sys, os, socket
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT
import zlib

# tiara.diamond doesn't have hashlib
#from hashlib import sha1
import sha
sha1 = sha.sha

# tiara.diamond has the older pysqlite-1.1.7
# to convert to the new api change sqlite to sqlite3 and replace the %s in
# the INSERT queries with ?
import sqlite

usage = "usage: %prog [options] servers+"
parser = OptionParser(usage=usage, description="""
    Volcano is a tool used to distribute a collections of objects across a
    group of servers. The objects are copied with sftp the decision where to
    place each object is based on the content hash which makes the process
    repeatable.
""")
parser.add_option("-v", "--verbose",
		  action="store_true", dest="verbose", default=False,
		  help="print more detailed status messages to stdout")
parser.add_option("-p", "--path",
		  action="append", dest="path",
		  help="include objects found below this directory (required, may be specified multiple times)")
parser.add_option("-c", "--collection",
		  action="store", dest="collection",
		  help="specify collection name, defaults to the name of the first path")
parser.add_option("-s", "--simple",
		  action="store_false", dest="use_sha1", default=True,
		  help="distribute based on the adler32 hash instead of sha1")
(options, servers) = parser.parse_args()

if not options.path:
  parser.error("no object source path specified")

if len(servers) == 0:
  parser.error("no servers specified")

nservers = len(servers)
if not options.collection:
    options.collection = os.path.basename(options.path[0])

USER="diamond"
GID="%s" % sha1(options.collection).hexdigest()[:16].upper()
diamond_gid = ':'.join(map(lambda x,y:x+y, GID[0::2], GID[1::2]))

provdb = sqlite.connect('provenance-%s.db' % options.collection)
provcur = provdb.cursor()
provcur.execute("""CREATE TABLE IF NOT EXISTS objects
		(groupid TEXT, server TEXT, oldpath TEXT, newpath TEXT)""")
provcur.execute("""CREATE TABLE IF NOT EXISTS collections
		(collection TEXT, groupid TEXT, localhost TEXT,
		 uid INTEGER, time TEXT, server TEXT)""")

#
# Wrapper around sftp, probably could be in it's own file.
#
SFTPCMD = "sftp"
class SFTPError(Exception):
    def __init__(self, server):
	self.server = server
    def __str__(self):
	return "sftp connection to %s closed unexpectedly" % self.server

class SFTP(Popen):
    def __init__(self, server, user=None):
	self.server = server
	if user: server = user + "@" + server
	Popen.__init__(self, [SFTPCMD, "-b-", server], stdin=PIPE, stdout=PIPE,
		       stderr=STDOUT, close_fds=True)
	self.lines = 0

    def __run(self, cmd, errout=True, async=128):
	if errout: err = ''
	else:	   err = '-'

	if self.poll(): raise SFTPError, self.server
	self.stdin.write(err + cmd + '\n')
	self.lines += 1 # sftp> cmd

	while self.lines > async:
	    if self.poll(): raise SFTPError, self.server
	    line = self.stdout.readline()
	    #print self.lines, line
	    if line[:6] == 'sftp> ': self.lines -= 1
	return self

    def __log(self, msg):
	if options.verbose:
	    print "%s: %s" % (self.server, msg)

    def cd(self, rpath, **kwargs):
	self.__log("Changing directory to %s" % rpath)
	return self.__run("cd %s" % rpath, **kwargs)

    def get(self, rpath, lpath, **kwargs):
	self.__log("Fetching %s to %s" % (rpath, lpath))
	self.lines += 1 # Fetching rpath to lpath ... sftp>
	return self.__run("get %s %s\n" % (rpath, lpath), **kwargs)

    def mkdir(self, rpath, **kwargs):
	self.__log("Creating directory %s" % rpath)
	return self.__run("mkdir %s" % rpath, **kwargs)

    def put(self, lpath, rpath, **kwargs):
	self.__log("Uploading %s to %s" % (lpath, rpath))
	self.lines += 1 # Uploading lpath to rpath ... sftp>
	return self.__run("put %s %s\n" % (lpath, rpath), **kwargs)

    def bye(self):
	self.__log("Disconnecting")
	self.__run("bye", async=0)
	self.wait()

class Server(SFTP):
    def __init__(self, server):
	SFTP.__init__(self, server, user=USER)

	conf = self.read_config()

	self.dataroot = conf['DATAROOT']
	self.indexfile = os.path.join(conf['INDEXDIR'], "GIDIDX%s" % GID)
	self.manifest = "GIDIDX.%s" % server
	try: os.unlink(self.manifest)
	except OSError: pass

	self.get(self.indexfile, self.manifest, errout=False, async=0)
	if os.path.exists(self.manifest):
	    print "%s: GID index file already existed, aborting" % server
	    os.unlink(self.manifest)
	    sys.exit(1)

	self.cd(self.dataroot)
	self.dirs = [ self.dataroot ]

	self.m = open(self.manifest, 'w')

    def read_config(self):
	print "%s: Fetching diamond_config" % self.server
	tmp_conf = "diamond_config.%s" % self.server
	try: os.unlink(tmp_conf)
	except OSError: pass

	self.get(".diamond/diamond_config", tmp_conf, async=0)
	conf = {}
	for line in open(tmp_conf):
	    try:
		line = line.strip()
		key, value = line.split(None, 1)
		conf[key] = value
	    except ValueError:
		pass
	os.unlink(tmp_conf)
	return conf

    def mkpath(self, dir):
	if not dir or dir in self.dirs: return
	self.mkpath(os.path.dirname(dir))
	self.dirs.append(dir)
	self.mkdir(dir, errout=False)

    def add(self, local_path, remote_path):
	self.mkpath(os.path.dirname(remote_path))
	self.put(local_path, remote_path)
	self.m.write(remote_path + '\n')
	provcur.execute("""INSERT INTO objects VALUES (%s, %s, %s, %s)""",
			diamond_gid, self.server, local_path, remote_path)

    def close(self):
	self.m.close()
	self.put(self.manifest, self.indexfile)
	self.bye()

# set up for server distribution, set up sftp connections to the servers,
# initialize manifest files and make sure the destination directories exist
conns = map(Server, servers)

def sha1sum(path):
    hash = sha1()
    f = open(path, 'rb')
    while 1:
	block = f.read(32768)
	if not block: break
	hash.update(block)
    f.close()
    return hash

def adler32(path):
    crc = 0
    f = open(path, 'rb')
    while 1:
	block = f.read(32768)
	if not block: break
	crc = zlib.adler32(block, crc)
    f.close()
    return crc & 0xffffffff

print "Counting number of files to send"
total = 0
for path in options.path:
    path = os.path.abspath(path)
    for root, dirs, files in os.walk(path):
	total += len(files)

# and push all files
count = 0
print "Starting file transfers"
for path in options.path:
    path = os.path.abspath(path)
    for root, dirs, files in os.walk(path):
	for file in files:
	    lpath = os.path.join(root, file)

	    if options.use_sha1:
		hash = sha1sum(lpath)
		sha1 = hash.hexdigest()
		idx = ord(hash.digest()[0])
		file, ext = os.path.splitext(file)
		rpath = "%s/%02x/%s%s" % (options.collection, idx, sha1, ext)
	    else:
		sha1 = ''
		idx = adler32(lpath)
		file, ext = os.path.splitext(file)
		rpath = "%s/%s%s" % (options.collection, file, ext)

	    # skip empty files
	    if sha1 != 'da39a3ee5e6b4b0d3255bfef95601890afd80709':
		conns[idx % nservers].add(lpath, rpath)

	    count += 1
	    if count % 10 == 0:
		cnt = "[%d/%d]" % (count, total)
		n = 75 - len(cnt)
		x = (n * count) / total
		print "\r%s |%s>%s|" % (cnt, '='*x, ' '*(n-x)),
print

metadb = sqlite.connect('metadata-%s.db' % options.collection)
metacur = metadb.cursor()
metacur.execute("""CREATE TABLE IF NOT EXISTS metadata
		(collection TEXT, groupid TEXT, server TEXT)""")

import socket, os, time
localhost = socket.gethostname()
uid = os.getuid()
timep = time.ctime()

for conn in conns:
    metacur.execute("""INSERT INTO metadata VALUES (%s, %s, %s)""",
		    options.collection, diamond_gid, conn.server)
    provcur.execute("""INSERT INTO collections VALUES (%s, %s, %s, %s, %s, %s)""",
		    options.collection, diamond_gid, localhost, uid, timep,
		    conn.server)
    conn.close()

metadb.commit()
metadb.close()
provdb.commit()
provdb.close()

