/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2008-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LIB_HSTUB_H_
#define	_LIB_HSTUB_H_

#include "dctl_impl.h"

typedef struct {
	void (*log_data_cb)	(void *hcookie, char *data, int len, int dev);
	void (*search_done_cb)	(void *hcookie);
	void (*conn_down_cb)	(void *hcookie);
} hstub_cb_args_t;

/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */

void *device_init(const char *host, void *cookie, hstub_cb_args_t *cbs);

int device_stop(void *dev, host_stats_t *hstats);
int device_terminate(void *dev);
int device_start(void *dev, unsigned int search_id);
int device_set_lib(void *dev, sig_val_t *sig);
int device_set_spec(void *dev, char *spec, sig_val_t *spec_sig);
int device_set_push_attrs(void *handle, const char **attrs);
int device_reexecute_filters(void *handle, obj_data_t *obj, const char **attrs);
int device_characteristics(void *handle, device_char_t *dev_chars);
int device_statistics(void *dev, dev_stats_t *dev_stats, int *stat_len);
int device_write_leaf(void *dev, char *path, int len, char *data);
int device_read_leaf(void *dev, char *path,
		     dctl_data_type_t *dtype, int *dlen, void *dval);
int device_list_nodes(void *dev, char *path, int *dlen, dctl_entry_t *dval);
int device_list_leafs(void *dev, char *path, int *dlen, dctl_entry_t *dval);
int device_set_scope(void *handle, const char *cookie);
int device_clear_scope(void *handle);
int device_set_blob(void *handle, char *name, int blob_len, void *blob);
int device_set_user_state(void *handle, uint32_t state);
int device_get_session_variables(void *handle, device_session_vars_t **vars);
int device_set_session_variables(void *handle, device_session_vars_t *vars);
obj_data_t * device_next_obj(void *handle);
void device_drain_objs(void *handle);

#endif	/* _LIB_HSTUB_H_ */

