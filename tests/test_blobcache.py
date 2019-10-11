#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2017 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from builtins import str
import logging

import opendiamond.blobcache


def test_blobcache_add(tmpdir):
    cache = opendiamond.blobcache.BlobCache(str(tmpdir))
    assert tmpdir.listdir() == []

    emptyfile = \
        b'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
    assert cache.add(b'') == emptyfile
    assert len(tmpdir.listdir()) == 1

    emptyfile_path = tmpdir.join(emptyfile)
    assert emptyfile_path.check(file=1)

    assert cache.add(b'') == emptyfile
    assert len(tmpdir.listdir()) == 1

    assert cache.add(b'test') == \
        b'9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08'
    assert len(tmpdir.listdir()) == 2


def test_blobcache_contains(tmpdir):
    cache = opendiamond.blobcache.BlobCache(str(tmpdir))
    assert tmpdir.listdir() == []

    emptyfile = \
        b'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
    assert cache.add(b'') == emptyfile
    assert len(tmpdir.listdir()) == 1

    assert emptyfile in cache


def test_blobcache_prune(tmpdir, caplog):
    cache = opendiamond.blobcache.BlobCache(str(tmpdir))
    assert tmpdir.listdir() == []

    emptyfile = \
        b'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
    assert cache.add(b'') == emptyfile
    assert len(tmpdir.listdir()) == 1

    with caplog.at_level(logging.INFO):
        opendiamond.blobcache.BlobCache.prune(str(tmpdir), 1)
    assert 'Pruned' not in caplog.text
    assert len(tmpdir.listdir()) == 1

    # make sure file is not using GC name
    emptyfile_path = tmpdir.join(emptyfile)
    assert emptyfile_path.check(file=1)

    with caplog.at_level(logging.INFO):
        opendiamond.blobcache.BlobCache.prune(str(tmpdir), 0)
    assert 'Pruned' in caplog.text
    assert len(tmpdir.listdir()) == 0


def test_blobcache_gc_rescue(tmpdir, caplog):
    cache = opendiamond.blobcache.BlobCache(str(tmpdir))
    assert tmpdir.listdir() == []

    emptyfile = \
        b'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
    assert cache.add(b'') == emptyfile
    assert len(tmpdir.listdir()) == 1

    emptyfile_path = tmpdir.join(emptyfile)
    gcfile_path = tmpdir.join(emptyfile + '-')
    emptyfile_path.rename(gcfile_path)

    assert not emptyfile_path.check(file=1)
    assert gcfile_path.check(file=1)

    # existence check rescues the file
    assert emptyfile in cache

    assert emptyfile_path.check(file=1)
    assert not gcfile_path.check(file=1)


def test_executable_blobcache(tmpdir, monkeypatch):
    execdir = tmpdir.mkdir('exec')
    cachedir = tmpdir.mkdir('cache')

    def mockreturn(*args, **kwargs):
        return str(execdir)
    monkeypatch.setattr(opendiamond.blobcache, 'mkdtemp', mockreturn)

    cache = opendiamond.blobcache.ExecutableBlobCache(str(cachedir))
    assert cachedir.listdir() == []

    emptyfile = \
        b'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'
    assert cache.add(b'') == emptyfile
    assert emptyfile in cache

    emptyfile_path = cachedir.join(emptyfile)
    assert emptyfile_path.check(file=1)

    filestat = emptyfile_path.stat()
    assert filestat.nlink == 1 and not filestat.mode & 0o111

    executable = cache.executable_path(emptyfile)
    executable_path = execdir.join(emptyfile)
    assert executable == str(executable_path)
    assert executable_path.check(file=1)

    # executable file will be a hardlink on unix systems
    filestat = emptyfile_path.stat()
    assert filestat.nlink == 2 and filestat.mode & 0o111

    # the original is changed too because iff we hardlinked
    filestat = emptyfile_path.stat()
    assert filestat.nlink == 2 and filestat.mode & 0o111
