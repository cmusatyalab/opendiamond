#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009-2019 Carnegie Mellon University
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
from django.shortcuts import render

from opendiamond.scope import generate_cookie_django

from .forms import CollectionForm, ManageForm
import json


@login_required
def index(request):
    if request.method == 'POST':
        form = CollectionForm(request.POST, user=request.user)

        if form.is_valid():
            cookie = []
            for collection in form.cleaned_data['collections']:
                scope = ["/collection/%s" % collection.gid.replace(':', '')]
                servers = set()
                for server in collection.servers.all():
                    servers.add(server.host)
                cookie.extend(generate_cookie_django(
                    scope, servers,
                    blaster=getattr(settings, 'GATEKEEPER_BLASTER', None)))

            return HttpResponse(cookie, content_type='application/x-diamond-scope')
    else:
        form = CollectionForm(user=request.user)

    return render(request, 'scopeserver/gatekeeper.html', {
        'form': form,
    })


@user_passes_test(lambda u: u.is_staff, redirect_field_name="/")
@login_required
def manage(request):
    if request.method == 'POST':
        form = ManageForm(request.POST)

        if form.is_valid():
            user = form.cleaned_data['user']
            user.collection_set = form.cleaned_data['collections']
            return HttpResponseRedirect("")

    elif request.is_ajax():
        coll = {}
        try:
            user = request.GET['user']
            user = User.objects.get(id=user)
            for c in user.collection_set.all():
                coll[c.id] = 1
        except Exception:  # pylint: disable=broad-except
            pass
        return HttpResponse(json.dumps(coll), content_type="application/json")
    else:
        form = ManageForm()

    return render(request, 'scopeserver/manage.html', {
        'form': form,
    })
