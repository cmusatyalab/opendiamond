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

from django.contrib.auth.decorators import login_required, user_passes_test
from django.contrib.auth.models import User
from django.http import HttpResponse, HttpResponseRedirect
from django.utils import simplejson
from django.shortcuts import render_to_response
from opendiamond.helpers import GenerateCookie
from forms import CollectionForm, ManageForm

@login_required
def index(request):
    if request.method == 'POST':
	form = CollectionForm(request.POST, user=request.user)

	if form.is_valid():
	  if request.POST.get('submit') == 'Define Scope':
	    collections = form.cleaned_data['collections']

	    gids = " ".join(c.gid for c in collections)
	    cookie = [ "1\ncollection %s\n%d\n" % (gids, len(collections)) ]
	    for c in collections:
		servers = " ".join(s.host for s in c.servers.all())
		cookie.append("%s %s\n" % (c.gid, servers))

	    resp = HttpResponse(cookie, mimetype='application/x-diamond-scope')
	    resp['Content-Disposition']='attachment; filename=diamond.scope'
	    return resp
	  else:
	    cookie = []
	    for collection in form.cleaned_data['collections']:
		scope = [ "/collection/%s" % collection.gid.replace(':','') ]
		servers = {}
		for server in collection.servers.all():
		    servers[server.host] = True
		cookie.extend(GenerateCookie(scope, servers))

	    resp = HttpResponse(cookie, mimetype='application/x-diamond-scope')
	    resp['Content-Disposition']='attachment; filename=opendiamond.scope'
	    return resp
    else:
	form = CollectionForm(user=request.user)

    return render_to_response('gatekeeper.html', {
	'form': form,
	'request': request,
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
	except:
	    pass
	return HttpResponse(simplejson.dumps(coll), mimetype="application/json")
    else:
	form = ManageForm()

    return render_to_response('gatekeeper_manage.html', {
	'form': form,
	'request': request,
    })

