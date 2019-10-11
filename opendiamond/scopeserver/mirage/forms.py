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

from builtins import zip
from django import forms
from django.conf import settings


class MirageForm(forms.Form):
    def __init__(self, *args, **kwargs):
        super(MirageForm, self).__init__(*args, **kwargs)
        self.fields['servers'].choices = list(zip(settings.MIRAGE_SERVERS,
                                             settings.MIRAGE_SERVERS))

    paths = forms.CharField(required=False, widget=forms.Textarea,
                            label="Specify file name patterns (one per line)")
    servers = forms.MultipleChoiceField(
        choices=(), required=False, label="Select one or more compute servers")
