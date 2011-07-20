#
#  The OpenDiamond Platform for Interactive Search
#  Version 4
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
from subprocess import Popen, PIPE

MIRAGE_REPOSITORY="/var/lib/mirage"

def MirageListPublished():
    p = Popen(['sudo','mg','list-published','-R',"file://"+MIRAGE_REPOSITORY],
	      stdout=PIPE, close_fds=True)
    for vmimage in p.stdout:
	vmimage = vmimage.strip()
        q = Popen(['sudo','mg','info','-R',"file://"+MIRAGE_REPOSITORY, vmimage],
		  stdout=PIPE, close_fds=True)

        name = q.stdout.readline().strip()
	q.stdout.close()

	vmimage = vmimage[22:]
	if not name: name = "Unnamed image: %s..." % vmimage[:9]
	yield (vmimage, name)

    p.stdout.close()

class MirageForm(forms.Form):
    def __init__(self, *args, **kwargs):
	super(MirageForm, self).__init__(*args, **kwargs)
	self.fields['vmimages'].choices = MirageListPublished()

    vmimages = forms.MultipleChoiceField(choices=(), required=False, label=
		    "Select one or more VM images")
    paths = forms.CharField(required=False, widget=forms.Textarea, label=
		    "Specify file name patterns (one per line)")

