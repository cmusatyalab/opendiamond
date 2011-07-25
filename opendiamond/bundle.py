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

import os
import subprocess
import zipfile

def bundle_generic(out, manifest, files):
    '''manifest is a string, files is a dict of filename => path pairs'''
    zip = zipfile.ZipFile(out, mode='w', compression=zipfile.ZIP_DEFLATED)
    zip.writestr('opendiamond-search.xml', manifest)
    for name, path in files.iteritems():
        zip.write(path, name)
    zip.close()


def bundle_python(out, search, additional=None):
    try:
        proc = subprocess.Popen(['python', os.path.realpath(search),
                            '--get-manifest'], stdout=subprocess.PIPE)
    except OSError:
        raise Exception("Couldn't execute search program")
    manifest = proc.communicate()[0]
    if proc.returncode != 0:
        raise Exception("Couldn't generate search manifest")
    files = {'filter': search}
    if additional is not None:
        files.update((os.path.basename(f), f) for f in additional)
    bundle_generic(out, manifest, files)
