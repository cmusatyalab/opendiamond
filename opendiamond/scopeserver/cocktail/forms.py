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
from opendiamond.scopeserver.gatekeeper.models import Collection
from .models import CocktailBase


class CocktailBaseForm(forms.Form):
    def __init__(self, *args, **kwargs):
        user = kwargs['user']
        del kwargs['user']
        super(CocktailBaseForm, self).__init__(*args, **kwargs)
        self.fields['bases'].queryset = user.collection_set.all()
        self.fields['mixers'].queryset = user.cocktailbase_set.all()

    bases = forms.ModelChoiceField(
        queryset=Collection.objects.all(),
        initial=1,
        widget=forms.RadioSelect
    )

    mixers = forms.ModelChoiceField(
        queryset=CocktailBase.objects.all(),
        empty_label='None',
        widget=forms.RadioSelect,
        required=False,
    )

    classes = forms.CharField(label='Classes: ',
                               help_text='Comma-separated Classes. '
                                         'All selected if left blank',
                               required=False)  # no 'strip' in Django 1.3

    TYPE_CHOICES = (
        ('d', 'DETERMINISTIC'),
        ('r', 'RANDOM'),
    )

    mixing_type = forms.ChoiceField(label='Type: ',
    							  widget=forms.Select(),
                                  choices=TYPE_CHOICES,
                                  initial='d')

    seed = forms.IntegerField(label='Seed: ',
                                 help_text='Integer',
                                 initial=42)

    percentage = forms.FloatField(label='Percentage: ',
                                  help_text='%',
                                  initial=1,
                                  min_value=0,
                                  max_value=100,
                                  required=False)


class ManageForm(forms.Form):
    user = forms.ModelChoiceField(queryset=User.objects.all())
    bases = forms.ModelMultipleChoiceField(
        queryset=CocktailBase.objects.all(),
        widget=forms.CheckboxSelectMultiple,
        required=False,
    )

    class Media(object):
        js = ('js/jquery.js',)
