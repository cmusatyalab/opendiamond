/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


/*
 * This provides many of the main functions in the provided
 * through the searchlet API.
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "lib_filter.h"
#include "lib_filter_sys.h"
#include "lib_filter_priv.h"
#include "lib_filterexec.h"
#include "lib_log.h"
#include "odisk_priv.h"
#include "filter_priv.h"
#include "sys_attr.h"


static read_attr_cb read_attr_fn = NULL;
static write_attr_cb write_attr_fn = NULL;


int
lf_set_read_cb(read_attr_cb cb_fn)
{
	read_attr_fn = cb_fn;
	return (0);
}

int
lf_internal_ref_attr(lf_obj_handle_t obj, const char *name, size_t * len, 
	unsigned char **data)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) obj;
	adata = &odata->attr_info;
	err = obj_ref_attr(adata, name, len, data);

	/* add read attrs into cache queue: input attr set */
	if (!err && (read_attr_fn != NULL)) {
		(*read_attr_fn) (obj, name, *len, *data);
	}
	return (err);
}

int
lf_set_write_cb(write_attr_cb cb_fn)
{
	write_attr_fn = cb_fn;
	return (0);
}

/*
 * Quick hacks for now.  Fix this later.
 * XXX
 */
int
lf_internal_write_attr(lf_obj_handle_t obj, char *name, size_t len, unsigned char *data)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) obj;
	adata = &odata->attr_info;
	err = obj_write_attr(adata, name, len, data);
	/*
	 * add writen attrs into cache queue: output attr set 
	 */
	if (!err && (write_attr_fn != NULL)) {
		(*write_attr_fn) (obj, name, len, data);
	}
	return (err);
}

int
lf_internal_omit_attr(lf_obj_handle_t obj, char *name)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) obj;
	adata = &odata->attr_info;
	err = obj_omit_attr(adata, name);

	return (err);
}

int
lf_internal_get_session_variables(lf_obj_handle_t ohandle,
				  lf_session_variable_t **list)
{
  obj_data_t *odata = (obj_data_t *) ohandle;
  session_variables_state_t *sv = odata->session_variables_state;

  if (sv == NULL) {
    return 0;
  }

  pthread_mutex_lock(&sv->mutex);

  // walk the list given, and fill in the values
  int i;
  for (i = 0; list[i] != NULL; i++) {
    lf_session_variable_t *cur = list[i];
    //printf(" looking up name: %s\n", cur->name);
    session_variable_value_t *svv = g_hash_table_lookup(sv->store, cur->name);
    if (svv == NULL) {
      cur->value = 0.0;
      continue;
    }

    // combine all values (between_get_and_set val will be 0 when not between)
    cur->value = svv->local_val + svv->global_val + svv->between_get_and_set_val;
    /*
    printf(" filter get '%s': %g # %g # %g -> %g\n",
	   cur->name,
	   svv->local_val, svv->global_val, svv->between_get_and_set_val,
	   cur->value);
    */
  }

  pthread_mutex_unlock(&sv->mutex);
  return 0;
}

int lf_internal_update_session_variables(lf_obj_handle_t ohandle,
					 lf_session_variable_t **list)
{
  obj_data_t *odata = (obj_data_t *) ohandle;
  session_variables_state_t *sv = odata->session_variables_state;

  if (sv == NULL) {
    return 0;
  }

  pthread_mutex_lock(&sv->mutex);

  // walk the list given, and update the values
  int i;
  for (i = 0; list[i] != NULL; i++) {
    lf_session_variable_t *cur = list[i];
    session_variable_value_t *svv = g_hash_table_lookup(sv->store, cur->name);
    if (svv == NULL) {
      svv = calloc(1, sizeof(session_variable_value_t));
      g_hash_table_replace(sv->store, strdup(cur->name), svv);
    }

    if (sv->between_get_and_set) {
      // the client has gotten local, but not set global
      // in this case, we only update between_get_and_set
      double val = svv->between_get_and_set_val + cur->value;
      /*
      printf(" filter set '%s': _ # %g # %g -> %g\n",
	     cur->name,
	     svv->between_get_and_set_val,
	     cur->value,
	     val);
      */
      svv->between_get_and_set_val = val;
    } else {
      // we are in sync with other clients, update local
      double val = svv->local_val + cur->value;
      /*
      printf(" filter set '%s': %g # _ # %g -> %g\n",
	     cur->name,
	     svv->local_val,
	     cur->value,
	     val);
      */
      svv->local_val = val;
    }
  }

  pthread_mutex_unlock(&sv->mutex);
  return 0;
}
