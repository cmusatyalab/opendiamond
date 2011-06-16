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

#include "lib_filter.h"
#include "lf_protocol.h"
#include "lf_priv.h"

/* maximum attribute name we allow */
#define MAX_ATTR_NAME 128

struct ohandle {
  GHashTable *attributes;
};

struct attribute {
  size_t len;
  void *data;
};

static void attribute_destroy(gpointer user_data) {
  struct attribute *attr = user_data;

  g_free(attr->data);
  g_slice_free(struct attribute, attr);
}

lf_obj_handle_t lf_obj_handle_new(void) {
  struct ohandle *ret = g_slice_new0(struct ohandle);

  ret->attributes = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, attribute_destroy);

  return ret;
}

void lf_obj_handle_free(lf_obj_handle_t obj) {
  struct ohandle *ohandle = obj;

  g_hash_table_unref(ohandle->attributes);
  g_slice_free(struct ohandle, ohandle);
}

static struct attribute *get_attribute(struct ohandle *ohandle,
                                       const char *name) {
  // look up in hash table
  struct attribute *attr = g_hash_table_lookup(ohandle->attributes,
					       name);

  // retrieve?
  if (attr == NULL) {
    lf_start_output();
    lf_send_tag(lf_state.out, "get-attribute");
    lf_send_string(lf_state.out, name);
    lf_end_output();

    int len;
    void *data = lf_get_binary(lf_state.in, &len);

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
  char *formatted_filter_name = g_strdup_printf("%s : ", lf_state.filter_name);

  va_start(ap, fmt);
  char *formatted_msg = g_strdup_vprintf(fmt, ap);
  va_end(ap);

  char *msg = g_strconcat(formatted_filter_name, formatted_msg, NULL);

  lf_start_output();
  lf_send_tag(lf_state.out, "log");
  lf_send_int(lf_state.out, level);
  lf_send_string(lf_state.out, msg);
  lf_end_output();

  g_free(msg);
  g_free(formatted_filter_name);
  g_free(formatted_msg);
}


int lf_read_attr(lf_obj_handle_t obj, const char *name, size_t *len,
		 void *data) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  struct attribute *attr = get_attribute(obj, name);

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
		const void **data) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  struct attribute *attr = get_attribute(obj, name);

  // found?
  if (attr == NULL) {
    return ENOENT;
  }

  *len = attr->len;
  *data = attr->data;

  return 0;
}

int lf_write_attr(lf_obj_handle_t ohandle, const char *name, size_t len,
		  const void *data) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  lf_start_output();
  lf_send_tag(lf_state.out, "set-attribute");
  lf_send_string(lf_state.out, name);
  lf_send_binary(lf_state.out, len, data);
  lf_end_output();

  return 0;
}

int lf_omit_attr(lf_obj_handle_t ohandle, const char *name) {
  if (strlen(name) + 1 > MAX_ATTR_NAME) {
    return EINVAL;
  }

  lf_start_output();
  lf_send_tag(lf_state.out, "omit-attribute");
  lf_send_string(lf_state.out, name);
  lf_end_output();

  // server sends false if non-existent
  return lf_get_boolean(lf_state.in) ? 0 : ENOENT;
}

int lf_get_session_variables(lf_obj_handle_t ohandle,
			     lf_session_variable_t **list) {
  lf_start_output();
  lf_send_tag(lf_state.out, "get-session-variables");

  // send the list of names
  //  g_message("* get session variables");
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    lf_send_string(lf_state.out, (*v)->name);
    //    g_message(" %s", (*v)->name);
  }
  lf_send_blank(lf_state.out);
  //  g_message(" ->");

  lf_end_output();

  // read in the values
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    (*v)->value = lf_get_double(lf_state.in);
    //    g_message(" %g", (*v)->value);
  }

  lf_get_blank(lf_state.in);

  return 0;
}

int lf_update_session_variables(lf_obj_handle_t ohandle,
				lf_session_variable_t **list) {
  lf_start_output();
  lf_send_tag(lf_state.out, "update-session-variables");

  // send the lists of names and values
  //  g_message("* update session variables");
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    lf_send_string(lf_state.out, (*v)->name);
    //    g_message(" %s", (*v)->name);
  }
  lf_send_blank(lf_state.out);
  for (lf_session_variable_t **v = list; *v != NULL; v++) {
    lf_send_double(lf_state.out, (*v)->value);
    //    g_message(" %g", (*v)->value);
  }
  lf_send_blank(lf_state.out);
  lf_end_output();

  return 0;
}
