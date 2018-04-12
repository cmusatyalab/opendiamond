#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2011 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from django.conf import settings
from django.contrib.auth.decorators import login_required, user_passes_test
from django.contrib.auth.models import User
from django.http import HttpResponse, HttpResponseRedirect
from django.utils import simplejson
from opendiamond.scope import generate_cookie_django
from opendiamond.scopeserver import render_response
from .forms import GodModeForm
import urllib


@login_required
def index(request):
    if request.method == 'POST':
        form = GodModeForm(request.POST, user=request.user)

        if form.is_valid():
            dr_url = form.cleaned_data['data_retriever_url'].strip()
            n_servers = form.cleaned_data['n_servers']

            servers_file = getattr(settings, 'DYNAMIC_SERVERS_FILE')
            servers = [l.strip() for l in open(servers_file, 'r').readlines()]
            servers = servers[:n_servers]

            cookie = []
            for ind, server in enumerate(servers):
                url_this_server = dr_url

                # substitute placeholders
                url_this_server = url_this_server.format(server_ind=ind,
                                                         server_ind_1=ind + 1)

                cookie.extend(
                    generate_cookie_django([urllib.quote(url_this_server)],
                                           [server], None))

            return HttpResponse(cookie, mimetype='application/x-diamond-scope')
    else:
        form = GodModeForm(user=request.user, initial={
            'data_retriever_url': '/proxy/{server_ind_1}of256/localhost:5873/yfcc100m/scope/cloudlet013.elijah.cs.cmu.edu',
            'n_servers': 8})
        form.fields[
            'data_retriever_url'].help_text += " Supported placeholders: {server_ind}, {server_ind_1}"

    return render_response(request, 'scopeserver/godmode.html', {
        'form': form,
    })
