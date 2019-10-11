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

from builtins import range
from django.contrib.auth.decorators import permission_required
from django.conf import settings
from django.http import QueryDict, HttpResponse
from django.shortcuts import render

from opendiamond.scope import generate_cookie_django

from .forms import MirageForm


@permission_required('mirage.search')
def index(request):
    if request.method == 'POST':
        form = MirageForm(request.POST)

        if form.is_valid():
            paths = form.cleaned_data.get('paths', '').split('\n')
            paths = [path.strip() for path in paths]  # trim whitespace
            paths = [path for path in paths if path]  # skip empty

            q = QueryDict('').copy()
            q.setlist('path', paths)
            query = q.urlencode()
            if query:
                query = "?" + query

            servers = form.cleaned_data['servers']

            cookie = []
            n = len(servers)
            for i in range(n):
                scope = ["%s/mirage/%dof%d%s" %
                         (settings.MIRAGE_DATARETRIEVER, i+1, n, query)]
                cookie.append(generate_cookie_django(
                    scope, (servers[i],),
                    blaster=getattr(settings, 'MIRAGE_BLASTER', None)))

            return HttpResponse(''.join(cookie), content_type='application/x-diamond-scope')
    else:
        form = MirageForm()

    return render(request, 'scopeserver/simple_form.html', {
        'form': form,
    })
