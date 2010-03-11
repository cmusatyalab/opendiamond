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
#include "filter-runner-util.h"

static struct attribute *get_attribute(FILE *in, FILE *out,
				       struct ohandle *ohandle, const char *name) {
  // look up in hash table
  struct attribute *attr = g_hash_table_lookup(ohandle->attributes,
					       name);

  // retrieve?
  if (attr == NULL) {
    start_output();
    send_tag(out, "get-attribute");
    send_string(out, name);
    end_output();

    int len;
    void *data = get_binary(in, &len);

    if (len == -1) {
      // no attribute
      return NULL;
    }

    attr = g_slice_new(struct attribute);
    attr->data = data;
    attr->len = len;

    g_hash_table_insert(ohandle->attributes, g_strdup(name), attr);
  }

  return attr;
}


void lf_log(int level, const char *fmt, ...) {
  va_list ap;
  char *formatted_filter_name = g_strdup_printf("%s : ", _filter_name);

  va_start(ap, fmt);
  char *formatted_msg = g_strdup_vprintf(fmt, ap);
  va_end(ap);

  char *msg = g_strconcat(formatted_filter_name, formatted_msg, NULL);

  start_output();
  send_tag(_out, "log");
  send_int(_out, level);
  send_string(_out, msg);
  end_output();

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

  start_output();
  send_tag(_out, "set-attribute");
  send_string(_out, name);
  send_binary(_out, len, data);
  end_output();

  return 0;
}

int lf_omit_attr(lf_obj_handle_t ohandle, char *name) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  start_output();
  send_tag(_out, "omit-attribute");
  send_string(_out, name);
  end_output();

  // server sends false if non-existent
  return get_boolean(_in) ? 0 : ENOENT;
}

int lf_get_session_variables(lf_obj_handle_t ohandle,
			     lf_session_variable_t **list) {
  start_output();
  send_tag(_out, "get-session-variables");

  // send the list of names
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    send_string(_out, (*v)->name);
  }
  send_blank(_out);

  end_output();

  // read in the values
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    (*v)->value = get_double(_in);
  }

  get_blank(_in);

  return 0;
}

int lf_update_session_variables(lf_obj_handle_t ohandle,
				lf_session_variable_t **list) {
  start_output();
  send_tag(_out, "update-session-variables");

  // send the lists of names and values
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    send_string(_out, (*v)->name);
  }
  send_blank(_out);
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    send_double(_out, (*v)->value);
  }
  send_blank(_out);
  end_output();

  return 0;
}
