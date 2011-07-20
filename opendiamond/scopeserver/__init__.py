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

from django.conf import settings
from django.shortcuts import render_to_response
from django.template import RequestContext

def render_response(request, template, map=None):
    '''A wrapper for render_to_response() that adds some additional variables
    to the template context without the need to configure a custom context
    processor.'''
    if map is None:
        map = dict()
    map = dict(map)
    map.update({
        'LOGIN_URL': settings.LOGIN_URL,
        'LOGOUT_URL': settings.LOGOUT_URL,
    })
    return render_to_response(template, map,
                        context_instance=RequestContext(request))
