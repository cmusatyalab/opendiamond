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
from opendiamond.scope import generate_cookie_django
from forms import MirageForm

@permission_required('access.search', login_url=settings.LOGIN_REDIRECT_URL)
def index(request):
    if request.method == 'POST':
	form = MirageForm(request.POST)

	if form.is_valid():
	    paths = form.cleaned_data.get('paths', '').split('\n')
	    paths = [ path.strip() for path in paths ] # trim whitespace
	    paths = [ path for path in paths if path ] # skip empty

	    users = form.cleaned_data.get('users', '').split('\n')
	    users = [ user.strip() for user in users ] # trim whitespace
	    users = [ user for user in users if user ] # skip empty

	    q = QueryDict('').copy()
	    q.setlist('path', paths)
	    q.setlist('user', users)
	    query = q.urlencode()
	    if query: query = "?" + query

	    scope = [ "/mirage/%s%s" % (image, query) for image in
			 form.cleaned_data['vmimages']]

	    cookie = generate_cookie_django(scope, settings.MIRAGE_SERVERS)
	    return HttpResponse(cookie, mimetype='application/x-diamond-scope')
    else:
	form = MirageForm()

    return render_to_response('simple_form.html', {
	'form': form,
	'request': request,
    })

