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
from __future__ import unicode_literals

from django.contrib import admin
from .models import CocktailBase, Server

# Register your models here.
admin.site.register(CocktailBase)
admin.site.register(Server)
