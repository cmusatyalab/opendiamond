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

from django.contrib.auth.decorators import permission_required
from django.conf import settings
from django.http import QueryDict, HttpResponse
from django.shortcuts import render

from opendiamond.scope import generate_cookie_django

from .forms import FlickrForm


@permission_required("flickr.search")
def index(request):
    if request.method == 'POST':
        form = FlickrForm(request.POST)

        if form.is_valid():
            tags = form.cleaned_data.get('tags', '').split('\n')
            tags = [tag.strip() for tag in tags]  # trim whitespace
            tags = [tag for tag in tags if tag]   # skip empty

            tag_mode = 'all' if form.cleaned_data['tag_mode'] else 'any'

            text = form.cleaned_data.get('text', '')

            q = QueryDict('').copy()
            if tags:
                q['tags'] = ','.join(tags)
                q['tag_mode'] = tag_mode
            if text:
                q['text'] = text
            query = q.urlencode()

            scope = ["/flickr/?%s" % query]

            proxies = settings.FLICKR_PROXIES \
                if form.cleaned_data['proxied'] else None
            cookie = generate_cookie_django(
                scope, settings.FLICKR_SERVERS, proxies,
                blaster=getattr(settings, 'FLICKR_BLASTER', None))

            return HttpResponse(cookie, content_type='application/x-diamond-scope')
    else:
        form = FlickrForm()

    return render(request, 'scopeserver/simple_form.html', {
        'form': form,
    })
