#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2009 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

from django import forms
from django.contrib.auth.models import User


class GodModeForm(forms.Form):
    def __init__(self, *args, **kwargs):
        user = kwargs['user']
        del kwargs['user']
        super(GodModeForm, self).__init__(*args, **kwargs)

    data_retriever_url = forms.CharField('DataRetriever URL',
                                         help_text='Free form DR URL to sign.',
                                         required=True)
    n_servers = forms.IntegerField('# Servers to use',
                                   help_text='Will sign for only the first N servers',
                                   required=True)
