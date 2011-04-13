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

import os
import subprocess
import zipfile

def make_zipfile(path, manifest, files):
    '''manifest is a string, files is a dict of filename => path pairs'''
    if os.path.exists(path):
        raise Exception("Refusing to clobber destination file")
    zip = zipfile.ZipFile(path, mode = 'w', compression = zipfile.ZIP_DEFLATED)
    zip.writestr('opendiamond-manifest.txt', manifest)
    for name, path in files.items():
        zip.write(path, name)
    zip.close()

def bundle_python(out, filter, blob = None):
    try:
        proc = subprocess.Popen(['python', os.path.realpath(filter),
                            '--get-manifest'], stdout = subprocess.PIPE)
    except OSError:
        raise Exception("Couldn't execute filter program")
    manifest = proc.communicate()[0]
    if proc.returncode != 0:
        raise Exception("Couldn't generate filter manifest")
    files = {'filter': filter}
    if blob is not None:
        files['blob'] = blob
    make_zipfile(out, manifest, files)
