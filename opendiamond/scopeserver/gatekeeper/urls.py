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

from django.conf.urls.defaults import patterns

urlpatterns = patterns(  # pylint: disable=invalid-name
    'opendiamond.scopeserver.gatekeeper.views',
    (r'^manage.html', 'manage'),
    (r'^$', 'index'),
)
