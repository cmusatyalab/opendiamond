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


class FlickrForm(forms.Form):
    tags = forms.CharField(widget=forms.Textarea,
                           help_text="Tags to search for (one per line)",
                           required=False)
    tag_mode = forms.BooleanField(label="Match all tags", required=False)
    text = forms.CharField(help_text="Searches title, description, and tags",
                           required=False)
    proxied = forms.BooleanField(label="Use proxies", required=False)
