#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2018-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from future import standard_library
standard_library.install_aliases()
from django.conf import settings
from django.contrib.auth.decorators import login_required, user_passes_test
from django.contrib.auth.models import User
from django.http import HttpResponse, HttpResponseRedirect
from django.shortcuts import render

from opendiamond.scope import generate_cookie_django

from .forms import CocktailBaseForm, ManageForm

import json
import urllib.request, urllib.parse, urllib.error

#bases, mixers, mixing_type, seed
@login_required
def index(request):
    if request.method == 'POST':
        form = CocktailBaseForm(request.POST, user=request.user)

        if form.is_valid():
            url_string = ''
            mixing_type = form.cleaned_data['mixing_type'].strip()
            if mixing_type:
                url_string += '/keywords/%s' % mixing_type[0]
            seed = form.cleaned_data['seed']
            url_string += '_{}'.format(seed)
            cookie = []
            collection = form.cleaned_data['bases']
            mixers = form.cleaned_data['mixers']
            server_all = set([server.host for server in collection.servers.all()])
            if mixers:
                percentage = form.cleaned_data['percentage']
                if percentage:
                    url_string += '_{}'.format(percentage)
                classes = form.cleaned_data['classes']
                if classes:
                    url_string += '/classes/{}'.format(classes)
                server_all = server_all & set([server.host for server in mixers.servers.all()])
                url_string = "/mixers/{}".format(mixers.dataset.replace(':', '').upper()) + url_string
            servers = server_all
            len_servers = len(servers)
            if mixers and len_servers > 1:
                for idx, server in enumerate(servers):
                    scope = [urllib.parse.quote("/cocktail/base/{}/distrbuted/{}of{}".format(
                    collection.gid.replace(':', '').upper(), (idx+1), len_servers) + url_string)]
                    cookie.extend(generate_cookie_django(
                        scope, [server],
                        blaster=getattr(settings, 'GATEKEEPER_BLASTER', None)))
            else:
                # If no mixing same url for all servers
                scope = [urllib.parse.quote("/cocktail/base/{}".format(
                            collection.gid.replace(':', '').upper()) + url_string)]
                cookie.extend(generate_cookie_django(
                    scope, servers,
                    blaster=getattr(settings, 'GATEKEEPER_BLASTER', None)))

            return HttpResponse(cookie, content_type='application/x-diamond-scope')
    else:
        form = CocktailBaseForm(user=request.user)

    return render(request, 'scopeserver/cocktail.html', {
        'form': form,
    })


@user_passes_test(lambda u: u.is_staff, redirect_field_name="/")
@login_required
def manage(request):
    if request.method == 'POST':
        form = ManageForm(request.POST)

        if form.is_valid():
            user = form.cleaned_data['user']
            user.cocktailbase_set = form.cleaned_data['bases']
            return HttpResponseRedirect("")

    elif request.is_ajax():
        coll = {}
        try:
            user = request.GET['user']
            user = User.objects.get(id=user)
            for c in user.cocktailbase_set.all():
                coll[c.id] = 1
        except Exception:  # pylint: disable=broad-except
            pass
        return HttpResponse(json.dumps(coll), content_type="application/json")
    else:
        form = ManageForm()

    return render(request, 'scopeserver/manage.html', {
        'form': form,
    })
