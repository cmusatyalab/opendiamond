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

from django import forms
from django.utils.html import mark_safe


class GigaPanCookieForm(forms.Form):
    gigapan_id = forms.CharField(required=True)


class GigaPanSearchForm(forms.Form):
    search = forms.CharField(label='Please enter a search phrase:',
                             required=True)


class GigaPanChoiceForm(forms.Form):
    gigapan_choice = forms.MultipleChoiceField(
        widget=forms.CheckboxSelectMultiple)

    def __init__(self, *args, **kwargs):
        ids = kwargs.pop('ids')
        super(GigaPanChoiceForm, self).__init__(*args, **kwargs)
        choices = []
        for id in ids:
            tag = ('<img src="http://www.gigapan.org/gigapans/' +
                   '%d-776x182.jpg">') % int(id)
            choices.append((id, mark_safe(tag)))
        self.fields['gigapan_choice'].choices = choices
