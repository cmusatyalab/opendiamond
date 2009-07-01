from django.db import models

class Access(models.Model):
    class Meta:
	permissions = ( ("search", "Can search photos through Flickr API"), )

