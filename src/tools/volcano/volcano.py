#!/usr/bin/python

import sys, os, socket
from hashlib import sha1
from itertools import cycle

servers=sys.argv[3:]
nservers = len(servers)
if not servers:
  print "Usage: volcano <collection> <path> <servers>+"
  sys.exit(1)

collection = sys.argv[1]
path = os.path.abspath(sys.argv[2])

USER="diamond"
GID="%s" % sha1(collection).hexdigest()[:16].upper()

#
# Wrapper around sftp, probably could be in it's own file.
#
from subprocess import Popen, PIPE, STDOUT
SFTPCMD = "/usr/bin/sftp"
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
	#print "%s: %s" % (self.server, msg)
	pass

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

print "Counting number of files to send"
total = 0
for root, dirs, files in os.walk(path):
    total += len(files)

# and push all files
count = 0
print "Starting file transfers"
for root, dirs, files in os.walk(path):
    for file in files:
	lpath = os.path.join(root, file)

	hash = sha1sum(lpath)
	idx = ord(hash.digest()[0])
	file, ext = os.path.splitext(lpath)
	rpath = "%s/%02x/%s%s" % (collection, idx, hash.hexdigest(), ext)

	conns[idx % nservers].add(lpath, rpath)
	count += 1
	if count % 10 == 0:
	    cnt = "[%d/%d]" % (count, total)
	    n = 75 - len(cnt)
	    x = (n * count) / total
	    print "\r%s |%s>%s|" % (cnt, '='*x, ' '*(n-x)),
print

diamond_gid = ':'.join(map(lambda x,y:x+y, GID[0::2], GID[1::2]))
print "[name_map]"
print collection, diamond_gid
print
print "[gid_map]"
print diamond_gid,
for conn in conns:
    print conn.server,
    conn.close()

