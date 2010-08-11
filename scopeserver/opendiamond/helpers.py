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

from subprocess import Popen, PIPE

def _GenerateCookie(scopelist, servers):
    cmd = ["cookiecutter"]
    for server in servers:
	cmd.extend(['-s', server])
    for url in scopelist:
	cmd.extend(['-u', url])
    return Popen(cmd, stdout=PIPE).stdout.read()

def GenerateCookie(scopelist, servers, proxies=None):
    if not proxies:
	return _GenerateCookie(scopelist, servers)
    cookie = []
    n = len(proxies)
    for i in range(n):
	scope = [ '/proxy/%dof%d/%s:5873%s' % (i+1, n, server, scope)
		  for scope in scopelist for server in servers ]
	cookie.append(_GenerateCookie(scope, (proxies[i],)))
    return ''.join(cookie)

