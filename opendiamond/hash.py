#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2012 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

'''Dummy module to keep pylint happy during VPATH builds, since pylint
reads the source directory and hash.so is in the build directory.'''

def murmur3_x64_128(_data, _seed=0):
    raise RuntimeError('opendiamond.hash not properly loaded')
