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

from django.contrib.auth.decorators import permission_required
from django.conf import settings
from django.http import QueryDict, HttpResponse
from django.shortcuts import render_to_response
from opendiamond.scope import generate_cookie
from forms import FlickrForm

@permission_required('access.search', login_url=settings.LOGIN_REDIRECT_URL)
def index(request):
    if request.method == 'POST':
	form = FlickrForm(request.POST)

	if form.is_valid():
	    tags = form.cleaned_data.get('tags', '').split('\n')
	    tags = [ tag.strip() for tag in tags ] # trim whitespace
	    tags = [ tag for tag in tags if tag ] # skip empty

	    tag_mode = form.cleaned_data['tag_mode'] and 'all' or 'any'

	    text = form.cleaned_data.get('text', '')

	    q = QueryDict('').copy()
	    if tags:
		q['tags'] = ','.join(tags)
		q['tag_mode'] = tag_mode
	    if text:
		q['text'] = text
	    query = q.urlencode()

	    scope = [ "/flickr/?%s" % query ]
	    servers = [ "westphal.isr.cs.cmu.edu" ]

	    cookie = generate_cookie(scope, servers)
	    resp = HttpResponse(cookie, mimetype='application/x-diamond-scope')
	    resp['Content-Disposition']='attachment; filename=opendiamond.scope'
	    return resp
    else:
	form = FlickrForm()

    return render_to_response('simple_form.html', {
	'form': form,
	'request': request,
    })

