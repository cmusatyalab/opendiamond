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

from builtins import object
from django import forms
from django.contrib.auth.models import User
from .models import MetadataCollection


class MetadataCollectionForm(forms.Form):
    def __init__(self, *args, **kwargs):
        user = kwargs['user']
        del kwargs['user']
        super(MetadataCollectionForm, self).__init__(*args, **kwargs)
        self.fields['collections'].queryset = user.metadatacollection_set.all()

    collections = forms.ModelMultipleChoiceField(
        queryset=MetadataCollection.objects.all(),
        widget=forms.CheckboxSelectMultiple
    )

    keywords = forms.CharField(label='Keywords',
                               help_text='Comma-separated phrases. '
                                         'Ignored if left blank',
                               required=False)  # no 'strip' in Django 1.3

    divisor = forms.IntegerField(label='Buckets',
                                 help_text='Integer. Bucket the images.'
                                           ' Ignored if left blank.',
                                 required=False)

    expression = forms.CharField(label="Expression",
                                 help_text='Condition applied to bucket ID.'
                                           ' Examples: "<3", "=3", ">3".'
                                           ' "3" will be converted to "=3".'
                                           ' Ignored if left blank.',
                                 required=False)


class ManageForm(forms.Form):
    user = forms.ModelChoiceField(queryset=User.objects.all())
    collections = forms.ModelMultipleChoiceField(
        queryset=MetadataCollection.objects.all(),
        widget=forms.CheckboxSelectMultiple,
        required=False,
    )

    class Media(object):
        js = ('js/jquery.js',)
