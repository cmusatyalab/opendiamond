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

def GenerateCookie(scope, servers):
    cmd = ["cookiecutter"]
    for server in servers:
        cmd.extend(['-s', server])
    for url in scope:
        cmd.extend(['-u', url])
    return Popen(cmd, stdout=PIPE).stdout


