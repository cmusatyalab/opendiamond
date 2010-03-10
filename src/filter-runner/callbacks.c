/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 5
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2010 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdarg.h>
#include <stdio.h>
#include <glib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "diamond_consts.h"

#include "lib_filter.h"
#include "filter-runner.h"
#include "util.h"


void lf_log(int level, const char *fmt, ...) {
  va_list ap;
  char *formatted_filter_name = g_strdup_printf("%s : ", _filter_name);

  va_start(ap, fmt);
  char *formatted_msg = g_strdup_vprintf(fmt, ap);
  va_end(ap);

  char *msg = g_strconcat(formatted_filter_name, formatted_msg, NULL);

  send_tag(_out, "log");
  send_int(_out, level);
  send_string(_out, msg);

  g_free(msg);
  g_free(formatted_filter_name);
  g_free(formatted_msg);
}


int lf_read_attr(lf_obj_handle_t obj, const char *name, size_t *len,
		 unsigned char *data) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  struct attribute *attr = get_attribute(_in, _out, obj, name);

  // found?
  if (attr == NULL) {
    return ENOENT;
  }

  // check size
  if (attr->len > *len) {
    *len = attr->len;
    return ENOMEM;
  }

  // copy it in
  *len = attr->len;
  memcpy(data, attr->data, attr->len);

  return 0;
}


int lf_ref_attr(lf_obj_handle_t obj, const char *name, size_t *len,
		unsigned char **data) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  struct attribute *attr = get_attribute(_in, _out, obj, name);

  // found?
  if (attr == NULL) {
    return ENOENT;
  }

  *len = attr->len;
  *data = attr->data;

  return 0;
}

int lf_write_attr(lf_obj_handle_t ohandle, char *name, size_t len,
		  unsigned char *data) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  send_tag(_out, "write-attribute");
  send_string(_out, name);
  send_binary(_out, len, data);
}

int lf_omit_attr(lf_obj_handle_t ohandle, char *name) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  send_tag(_out, "omit-attribute");
  send_string(_out, name);

  // server sends false if non-existent
  return get_boolean(_in) ? 0 : ENOENT;
}

int lf_first_attr(lf_obj_handle_t ohandle, char **name,
		  size_t *len, unsigned char **data, void **cookie) {
  // TODO
}

int lf_next_attr(lf_obj_handle_t ohandle, char **name,
		 size_t *len, unsigned char **data, void **cookie) {
  // TODO
}

int lf_get_session_variables(lf_obj_handle_t ohandle,
			     lf_session_variable_t **list) {
  // TODO
}

int lf_update_session_variables(lf_obj_handle_t ohandle,
				lf_session_variable_t **list) {
  // TODO
}
