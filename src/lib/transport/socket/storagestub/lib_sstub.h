/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2008 Carnegie Mellon University
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
	int	(*start_cb)	(void *app_cookie, int gen);
	int	(*stop_cb)	(void *app_cookie, int gen, host_stats_t *stat);
	int	(*set_fspec_cb)	(void *app_cookie, int gen, sig_val_t *specsig);
	int	(*set_fobj_cb)	(void *app_cookie, int gen, sig_val_t *obj_sig);
	int	(*terminate_cb)	(void *app_cookie, int gen);
dev_stats_t*	(*get_stats_cb) (void *app_cookie, int gen);
	int	(*release_obj_cb) (void *app_cookie, obj_data_t *obj);
device_char_t*	(*get_char_cb)	(void *app_cookie, int gen);
	int	(*setlog_cb)	(void *app_cookie, uint32_t lvl, uint32_t src);
dctl_rleaf_t*	(*rleaf_cb)	(void *app_cookie, char *path);
	int	(*wleaf_cb)	(void *app_cookie, char *path, int len,
				 char *data);
dctl_lleaf_t*	(*lleaf_cb)	(void *app_cookie, char *path);
dctl_lnode_t*	(*lnode_cb)	(void *app_cookie, char *path);
	int	(*sgid_cb)	(void *app_cookie, int gen, groupid_t gid);
	int	(*clear_gids_cb) (void *app_cookie, int gen);
	int	(*set_blob_cb)	(void *app_cookie, int gen, char *name,
				 int blen, void *blob);
	int	(*set_exec_mode_cb) (void *app_cookie, uint32_t mode);
	int	(*set_user_state_cb) (void *app_cookie, uint32_t state);
device_session_vars_t* (*get_session_vars_cb) (void *app_cookie, int gen);
	int	(*set_session_vars_cb) (void *app_cookie, int gen,
					device_session_vars_t *vars);
} sstub_cb_args_t;


void * sstub_init(sstub_cb_args_t *cb_args);
void * sstub_init_2(sstub_cb_args_t *cb_args, int bind_only_locally);
void * sstub_init_ext(sstub_cb_args_t *cb_args, 
						int bind_only_locally,
						int auth_required);
void  sstub_listen(void * cookie);
int sstub_get_partial(void *cookie, obj_data_t **obj);
int sstub_flush_objs(void *cookie, int vnum);
int sstub_send_obj(void *cookie, obj_data_t *obj, int vnum, int complete);
int sstub_send_log(void *cookie, char *buf, int len);
float sstub_get_drate(void *cookie);
int sstub_queued_objects(void *cookie);
void sstub_get_conn_info(void *cookie, session_info_t *sinfo);

#endif /* !_LIB_SSTUB_H_ */

