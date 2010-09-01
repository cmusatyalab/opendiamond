/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 5
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2009 Carnegie Mellon University
 *
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LIB_SSTUB_H_
#define	_LIB_SSTUB_H_

/*
 * Callback functions that are passed when initializing the library.
 */
typedef struct {
	int	(*new_conn_cb)	(void *cookie, void **app_cookie);
	int	(*close_conn_cb) (void *app_cookie);
	int	(*start_cb)	(void *app_cookie, unsigned int search_id);
	int	(*set_fspec_cb)	(void *app_cookie, sig_val_t *specsig);
	int	(*set_fobj_cb)	(void *app_cookie, sig_val_t *obj_sig);
dev_stats_t*	(*get_stats_cb) (void *app_cookie);
	int	(*release_obj_cb) (void *app_cookie, obj_data_t *obj);
	int	(*set_blob_cb)	(void *app_cookie, char *name,
				 int blen, void *blob);
device_session_vars_t* (*get_session_vars_cb) (void *app_cookie);
	int	(*set_session_vars_cb) (void *app_cookie,
					device_session_vars_t *vars);
	obj_data_t *(*reexecute_filters) (void *app_cookie, const char *obj_id);
	int	(*set_scope_cb)	(void *app_cookie, const char *scope);
} sstub_cb_args_t;

void *sstub_init(sstub_cb_args_t *cb_args, int bind_only_locally);

void sstub_listen(void * cookie);

int sstub_get_partial(void *cookie, obj_data_t **obj);

int sstub_flush_objs(void *cookie);

int sstub_send_obj(void *cookie, obj_data_t *obj, int complete);

float sstub_get_drate(void *cookie);

void sstub_get_conn_info(void *cookie, session_info_t *sinfo);

#endif /* !_LIB_SSTUB_H_ */

