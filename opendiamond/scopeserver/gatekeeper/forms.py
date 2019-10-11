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

from builtins import object
from django import forms
from django.contrib.auth.models import User
from .models import Collection


class CollectionForm(forms.Form):
    def __init__(self, *args, **kwargs):
        user = kwargs['user']
        del kwargs['user']
        super(CollectionForm, self).__init__(*args, **kwargs)
        self.fields['collections'].queryset = user.collection_set.all()
    collections = forms.ModelMultipleChoiceField(
        queryset=Collection.objects.all(),
        widget=forms.CheckboxSelectMultiple
    )


class ManageForm(forms.Form):
    user = forms.ModelChoiceField(queryset=User.objects.all())
    collections = forms.ModelMultipleChoiceField(
        queryset=Collection.objects.all(),
        widget=forms.CheckboxSelectMultiple,
        required=False,
    )

    class Media(object):
        js = ('js/jquery.js',)
