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

from django.db import models
from django.contrib.auth.models import User


class Server(models.Model):
    host = models.CharField(max_length=100)

    def __unicode__(self):
        return self.host


class CocktailBase(models.Model):
    name = models.CharField(max_length=256)
    dataset = models.CharField(max_length=256)
    allowed_users = models.ManyToManyField(User, blank=True)
    servers = models.ManyToManyField(Server)

    def __unicode__(self):
        return self.name
