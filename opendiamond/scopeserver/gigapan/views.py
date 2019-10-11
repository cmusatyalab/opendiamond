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

from future import standard_library
standard_library.install_aliases()
from builtins import str
from urllib.parse import quote_plus
from urllib.request import urlopen
from urllib.error import HTTPError

from django.contrib.auth.decorators import permission_required
from django.conf import settings
from django.http import HttpResponse, HttpResponseRedirect
from django.shortcuts import redirect, render
from django.urls import reverse

from opendiamond.scope import generate_cookie_django

from .forms import GigaPanSearchForm, GigaPanChoiceForm

try:
    import json
except ImportError:
    import simplejson as json


def generate_cookie(ids):
    def full_url(gigapan):
        return 'http://%s:5873%s' % (settings.GIGAPAN_SERVER, gigapan)
    gigapans = ["/gigapan/%d" % int(gigapan_id) for gigapan_id in ids]
    proxies = settings.GIGAPAN_PROXIES
    blaster = getattr(settings, 'GIGAPAN_BLASTER', None)
    if len(proxies) > len(gigapans):
        mapping = {}  # gigapan -> [proxy]
        gigapan_index = 0
        for proxy in proxies:
            mapping.setdefault(gigapans[gigapan_index], []).append(proxy)
            gigapan_index = (gigapan_index + 1) % len(gigapans)
        cookies = []
        for gigapan in gigapans:
            if len(mapping[gigapan]) > 1:
                cookies.append(generate_cookie_django(
                    [gigapan], servers=[settings.GIGAPAN_SERVER],
                    proxies=mapping[gigapan], blaster=blaster))
            else:
                cookies.append(generate_cookie_django(
                    [full_url(gigapan)], mapping[gigapan], blaster=blaster))
        cookie = ''.join(cookies)
    else:
        mapping = {}  # proxy -> [gigapan]
        proxy_index = 0
        for gigapan in gigapans:
            mapping.setdefault(proxies[proxy_index],
                               []).append(full_url(gigapan))
            proxy_index = (proxy_index + 1) % len(proxies)
        cookie = ''.join([generate_cookie_django(mapping[proxy], [proxy],
                          blaster=blaster) for proxy in proxies])

    return HttpResponse(cookie, content_type='application/x-diamond-scope')


@permission_required("gigapan.search")
def generate(request):
    '''Generate cookie'''
    # hack to defeat validation of set membership
    form = GigaPanChoiceForm(request.POST or None,
                             ids=request.POST.getlist('gigapan_choice'))
    if form.is_valid():
        return generate_cookie(form.cleaned_data['gigapan_choice'])
    return redirect('index')


@permission_required("gigapan.search")
def browse(request):
    '''Parse search form, perform search'''
    form = GigaPanSearchForm(request.GET or None)
    if form.is_valid():
        query = form.cleaned_data.get('search')
        if query.isdigit():
            api_url = ('http://api.gigapan.org/beta/gigapans/%d.json' %
                       int(query))
            try:
                # Check that the ID is valid
                urlopen(api_url)
                ids = [query]
            except HTTPError:
                ids = []
        else:
            url = "http://api.gigapan.org/beta/gigapans/page/1/matching/"
            url += "%s/most_popular.json" % quote_plus(query)
            text = str(urlopen(url).read())
            data = json.loads(text)
            ids = [id for id, _ in data[u'items']]

        if ids:
            choiceform = GigaPanChoiceForm(ids=ids)
            return render(request, 'scopeserver/gigapan_browse.html', {
                'form': choiceform
            })
        return HttpResponseRedirect(reverse('index') + "?error=True")
    return redirect('index')


@permission_required("gigapan.search")
def index(request):
    '''Generate search form'''
    form = GigaPanSearchForm()
    if request.GET:
        return render(request, 'scopeserver/gigapan_search.html', {
            'form': form,
            'errors': "No results found",
        })
    return render(request, 'scopeserver/gigapan_search.html', {
        'form': form,
    })
