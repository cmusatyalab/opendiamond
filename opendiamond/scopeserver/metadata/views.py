#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2018 Carnegie Mellon University
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
from .forms import MetadataCollectionForm, ManageForm
import urllib

@login_required
def index(request):
    if request.method == 'POST':
        form = MetadataCollectionForm(request.POST, user=request.user)

        if form.is_valid():
            append = ''
            keywords = form.cleaned_data['keywords'].strip()
            if keywords:
                append += '/keywords/%s' % keywords
            divisor = form.cleaned_data['divisor']
            expression = form.cleaned_data['expression']
            if divisor and expression:
                try:
                    expr1 = "=%d" % int(expression)
                except ValueError:
                    expr1 = expression
                append += '/modulo/%d/%s' % (divisor, expr1)

            cookie = []
            for collection in form.cleaned_data['collections']:
                scope = [urllib.quote(("/mysql/v1/scope/%s" % collection.dataset) + append)]
                servers = set()
                for server in collection.servers.all():
                    servers.add(server.host)
                cookie.extend(generate_cookie_django(
                    scope, servers,
                    blaster=getattr(settings, 'GATEKEEPER_BLASTER', None)))

            return HttpResponse(cookie, mimetype='application/x-diamond-scope')
    else:
        form = MetadataCollectionForm(user=request.user)

    return render_response(request, 'scopeserver/metadata.html', {
        'form': form,
    })


@user_passes_test(lambda u: u.is_staff, redirect_field_name="/")
@login_required
def manage(request):
    if request.method == 'POST':
        form = ManageForm(request.POST)

        if form.is_valid():
            user = form.cleaned_data['user']
            user.metadatacollection_set = form.cleaned_data['collections']
            return HttpResponseRedirect("")

    elif request.is_ajax():
        coll = {}
        try:
            user = request.GET['user']
            user = User.objects.get(id=user)
            for c in user.metadatacollection_set.all():
                coll[c.id] = 1
        except Exception:  # pylint: disable=broad-except
            pass
        return HttpResponse(simplejson.dumps(coll),
                            mimetype="application/json")
    else:
        form = ManageForm()

    return render_response(request, 'scopeserver/manage.html', {
        'form': form,
    })
