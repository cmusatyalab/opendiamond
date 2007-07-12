/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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
#include "lib_log.h"


static read_attr_cb read_attr_fn = NULL;
static write_attr_cb write_attr_fn = NULL;


int
lf_set_read_cb(read_attr_cb cb_fn)
{
	read_attr_fn = cb_fn;
	return (0);
}

int
lf_read_attr(lf_obj_handle_t obj, const char *name, size_t * len, 
	unsigned char *data)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) obj;
	adata = &odata->attr_info;
	err = obj_read_attr(adata, name, len, data);

	/*
	 * pass information about the read to the cache function 
	 */
	if (!err && (read_attr_fn != NULL)) {
		(*read_attr_fn) (obj, name, *len, data);
	}
	return (err);
}


int
lf_ref_attr(lf_obj_handle_t obj, const char *name, size_t * len, 
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
lf_write_attr(lf_obj_handle_t obj, char *name, size_t len, unsigned char *data)
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
lf_omit_attr(lf_obj_handle_t obj, char *name)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) obj;
	adata = &odata->attr_info;
	err = obj_omit_attr(adata, name);

	return (err);
}

/*
 * This gets the next blocks of data in an object.  The block size is
 * specified by the filter specification.  
 *
 *
 * Args:
 *      obj_handle - the object handle.
 *
 *      num_blocks - the number of blocks to read.
 *
 *      len        - This is a pointer to the number of bytes to read.  On
 *                   return this location will hold the number of bytes that
 *                   were actually read.
 *
 *      bufp       - a pointer to the location where the buffer pointer
 *                   will be stored.
 *
 * Return:
 *      0          - the read was successful. 
 *
 *      ENOSPC     - insufficient resources were available to complete the call.
 * 
 *      ENOENT     - insufficient resources were available to complete the call.
 * 
 *      EINVAL     - one of the handles was invalid. 
 *
 */

int
lf_next_block(lf_obj_handle_t obj_handle,
	      int num_blocks, size_t * len, unsigned char **bufp)
{
	obj_data_t     *odata;
	unsigned char  *buf;
	size_t          length;
	size_t          remain;
	int             max_blocks;

	odata = (obj_data_t *) obj_handle;

	/*
	 * See if there is any data to read.
	 */
	if (odata->data_len <= odata->cur_offset) {
		printf("Beyond file len: off %lx len %lx \n",
		       odata->cur_offset, odata->data_len);
		*len = 0;
		assert(0);
		return (EINVAL);
	}

	/*
	 * We need to make sure that the product will not overflow
	 * the off_t.
	 */
	max_blocks = INT_MAX / odata->cur_blocksize;
	if (num_blocks >= max_blocks) {
		length = INT_MAX;
	} else {
		length = num_blocks * odata->cur_blocksize;
	}
	remain = odata->data_len - odata->cur_offset;
	if (length > remain) {
		length = remain;
	}
#ifdef	DEBUG_COPY
	buf = (char *) malloc(length);
	if (buf == NULL) {
		printf("failed to allocate block \n");
		assert(0);
		*len = 0;
		return (ENOSPC);
	}

	memcpy(buf, &odata->data[odata->cur_offset], length);
#else
	buf = (unsigned char *)&odata->data[odata->cur_offset];
#endif
	odata->cur_offset += length;
	*len = length;
	*bufp = buf;

	return (0);
}


/*
 * This skips the next N blocks of the specified object.  For an input
 * object, this will effect the data retrieved the next time lf_next_block()
 * is called.  XXX specify what happens on output objects if we aren't
 * currently aligned on the block boundary.
 *
 *
 * Args:
 *      obj_handle - the object handle.
 *
 *      num_blocks - the number of blocks to read.
 *
 * Return:
 *      0          - the read was successful. 
 *
 *      ENOSPC     - insufficient resources were available to complete the call.
 * 
 *      EINVAL     - one of the handles was invalid. 
 *
 */

int
lf_skip_block(lf_obj_handle_t obj_handle, int num_blocks)
{
	obj_data_t     *odata;
	size_t          length;
	size_t          remain;

	odata = (obj_data_t *) obj_handle;

	/*
	 * See if there is any data to skip past.  
	 */
	if (odata->data_len <= odata->cur_offset) {
		return (ENOENT);
	}

	length = num_blocks * odata->cur_blocksize;
	remain = odata->data_len - odata->cur_offset;

	if (length > remain) {
		length = remain;
	}

	odata->cur_offset += length;
	return (0);
}


/*
 * This function allows the programmer to log some data that
 * can be retrieved from the host system.
 *
 * Args:  
 *      level      - The log level associated with the command.  This
 *                   used to limit the amount of information being passed.
 *
 *      name       - The name of the attribute to write.
 *
 *      fmt        - format string used for parsing the data.  This uses
 *                   printf syntax
 *
 *      ...        - the arguments for the format.
 *
 */
/*
 * XXX this should match the one in log, but doesn't need to 
 */
#define	MAX_LOG_BUF	80

char           *fexec_cur_filtname();

void
lf_log(int level, char *fmt, ...)
{
	va_list         ap;
	va_list         new_ap;
	char            log_buffer[MAX_LOG_BUF];
	char           *cur_filter;
	int             len;
	int             remain_len;

	cur_filter = fexec_cur_filtname();
	len = snprintf(log_buffer, MAX_LOG_BUF, "%s : ", cur_filter);
	assert((len > 0) || (len < MAX_LOG_BUF));

	remain_len = MAX_LOG_BUF - len;

	va_start(ap, fmt);
	va_copy(new_ap, ap);
	vsnprintf(&log_buffer[len], remain_len, fmt, new_ap);
	va_end(ap);

	log_buffer[MAX_LOG_BUF - 1] = '\0';

	log_message(LOGT_APP, level, "%s", log_buffer);

}


int 
lf_first_attr(lf_obj_handle_t ohandle, char **name,
                size_t *len, unsigned char **data, void **cookie)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) ohandle;
	adata = &odata->attr_info;
	err = obj_first_attr(adata, name, len, data, cookie);
	return (err);
}

int 
lf_next_attr(lf_obj_handle_t ohandle, char **name,
                size_t *len, unsigned char **data, void **cookie)
{
	obj_data_t     *odata;
	obj_attr_t     *adata;
	int             err;

	odata = (obj_data_t *) ohandle;
	adata = &odata->attr_info;
	err = obj_next_attr(adata, name, len, data, cookie);
	return (err);
}


int
lf_get_session_variables(lf_obj_handle_t ohandle,
			 lf_session_variable_t **list)
{
  obj_data_t *odata = (obj_data_t *) ohandle;
  session_variables_state_t *sv = odata->session_variables_state;

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
    session_variable_composer_fn cf = cur->composer;
    cur->value = cf(cf(svv->local_val, svv->global_val),
		    svv->between_get_and_set_val);
  }

  pthread_mutex_unlock(&sv->mutex);
  return 0;
}

int lf_update_session_variables(lf_obj_handle_t ohandle,
				lf_session_variable_t **list)
{
  obj_data_t *odata = (obj_data_t *) ohandle;
  session_variables_state_t *sv = odata->session_variables_state;

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

    session_variable_composer_fn cf = cur->composer;
    if (sv->between_get_and_set) {
      // the client has gotten local, but not set global
      // in this case, we only update between_get_and_set
      svv->between_get_and_set_val =
	cf(svv->between_get_and_set_val, cur->value);
    } else {
      // we are in sync with other clients, update local
      svv->local_val = cf(svv->local_val, cur->value);
    }
  }

  pthread_mutex_unlock(&sv->mutex);
  return 0;
}
