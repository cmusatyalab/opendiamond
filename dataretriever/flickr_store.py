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
#
# Functions to access images through the Flickr API
#

from urllib import unquote_plus
from urllib2 import urlopen
from wsgiref.util import shift_path_info
from string import maketrans
import flickrapi

baseurl = 'flickr'
api_key = ''
flickr = flickrapi.FlickrAPI(api_key)

def scope_app(environ, start_response):
  root = shift_path_info(environ)
  if root == 'obj':
    return object_app(environ, start_response)

  search_params = {} # parse query string
  qs = environ.get('QUERY_STRING', "").split('&')
  ql = [ map(unquote_plus, comp.split('=', 1)) for comp in qs if '=' in comp ]
  search_params.update(ql)

  search_params['media'] = "photos"
  search_params['sort'] = "date-posted-asc"

  start_response("200 OK", [('Content-Type', "text/xml")])
  return FlickrObjList(search_params)

def FlickrObjList(search_params):
  nphotos = 0
  yield '<?xml version="1.0" encoding="UTF-8" ?>\n'
  yield '<objectlist>\n'
  while 1:
    photos = flickr.photos_search(**search_params).find('photos')

    try:
      total = int(photos.attrib['total'])
      if nphotos != total:
        yield '<count adjust="%d"/>\n' % (total - nphotos)
        nphotos = total
    except (KeyError, ValueError):
      pass

    for photo in photos.findall('photo'):
      yield '<object src="obj/%s"/>\n' % photo.attrib['id']

    page = int(photos.attrib['page'])
    pages = int(photos.attrib['pages'])
    if page >= pages: break
    search_params['page'] = page + 1
  yield '</objectlist>'


control_chars = [ chr(x) for x in range(32) ] + ['\x7f']
stripcontrol = maketrans(''.join(control_chars), ' ' * len(control_chars))

def http_strip(text):
  return text.encode('ascii', 'replace').translate(stripcontrol)

def object_app(environ, start_response):
  photo_id = environ['PATH_INFO'][1:]

  sizes = flickr.photos_getsizes(photo_id=photo_id).find('sizes')
  for img in sizes:
    url = img.attrib['source']
    if img.attrib['label'] == 'Medium':
      break

  obj = urlopen(url)

  info = flickr.photos_getinfo(photo_id=photo_id).find('photo')
  title = info.find('title').text or ''
  desc = info.find('description').text or ''
  date_taken = info.find('dates').attrib['taken']
  tags = ','.join(tag.text for tag in info.find('tags').findall('tag')) or ''

  headers = [ # copy some headers for caching purposes
    ('Content-Length',		obj.headers['Content-Length']),
    ('Content-Type',		obj.headers['Content-Type']),
    ('Last-Modified',		obj.headers['Last-Modified']),
    ('x-attr-title',		http_strip(title)),
    ('x-attr-description',	http_strip(desc)),
    ('x-attr-date-taken',	http_strip(date_taken)),
    ('x-attr-tags',		http_strip(tags)),
  ]
  start_response("200 OK", headers)
  return environ['wsgi.file_wrapper'](obj, 65536)

