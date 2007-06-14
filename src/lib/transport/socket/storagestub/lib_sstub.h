/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University 
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
 * Typedef's for the callback functions that are passed when initializing
 * the library.
 */

typedef	int (*sstub_new_conn_fn)(void *cookie, void **app_cookie);
typedef	int (*sstub_close_conn_fn)(void *app_cookie);
typedef	int (*sstub_start_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_stop_fn)(void *app_cookie, int gen_num, 
		host_stats_t *hstats);
typedef	int (*sstub_set_filter_spec_fn)(void *app_cookie, int gen_num, 
		sig_val_t *spec_sig);
typedef	int (*sstub_set_filter_obj_fn)(void *app_cookie, int gen_num, 
		sig_val_t *obj_sig);

typedef	int (*sstub_set_list_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_terminate_fn)(void *app_cookie, int gen_num);
typedef	dev_stats_t *(*sstub_getstats_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_release_obj_fn)(void *app_cookie, obj_data_t * obj);
typedef	device_char_t *(*sstub_get_devchar_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_log_done_fn)(void *app_cookie, char *buf, int len);
typedef	int (*sstub_set_log_fn)(void *app_cookie, uint32_t level, uint32_t src);

typedef	int (*sstub_rleaf_fn)(void *app_cookie, char *path, int32_t opid);
typedef	int (*sstub_wleaf_fn)(void *app_cookie, char *path, int len,
                              char *data, int32_t opid);
typedef	int (*sstub_lleaf_fn)(void *app_cookie, char *path, int32_t opid);
typedef	int (*sstub_lnode_fn)(void *app_cookie, char *path, int32_t opid);
typedef	int (*sstub_sgid_fn)(void *app_cookie, int gen_num, groupid_t gid);
typedef	int (*sstub_clear_gids_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_set_blob_fn)(void *app_cookie, int gen_num, char * name,
                                 int blen, void *blob);
typedef int (*sstub_set_offload_fn)(void *app_cookie, int gen_num,
                                    uint64_t load);
typedef int (*sstub_set_exec_mode_fn)(void *app_cookie, uint32_t mode);
typedef int (*sstub_set_user_state_fn)(void *app_cookie, uint32_t state);


typedef struct {
	sstub_new_conn_fn 	    	new_conn_cb;
	sstub_close_conn_fn 		close_conn_cb;
	sstub_start_fn 		    	start_cb;
	sstub_stop_fn 		    	stop_cb;
	sstub_set_filter_spec_fn	set_fspec_cb;
	sstub_set_filter_obj_fn		set_fobj_cb;
	sstub_set_list_fn	    	set_list_cb;
	sstub_terminate_fn	    	terminate_cb;
	sstub_getstats_fn	    	get_stats_cb;
	sstub_release_obj_fn		release_obj_cb;
	sstub_get_devchar_fn		get_char_cb;
	sstub_log_done_fn	    	log_done_cb;
	sstub_set_log_fn	    	setlog_cb;
	sstub_rleaf_fn	        	rleaf_cb;
	sstub_wleaf_fn	        	wleaf_cb;
	sstub_lleaf_fn	        	lleaf_cb;
	sstub_lnode_fn	        	lnode_cb;
	sstub_sgid_fn	        	sgid_cb;
	sstub_clear_gids_fn			clear_gids_cb;
	sstub_set_blob_fn			set_blob_cb;
	sstub_set_offload_fn    	set_offload_cb;
	sstub_set_exec_mode_fn 		set_exec_mode_cb;
	sstub_set_user_state_fn 	set_user_state_cb;
} sstub_cb_args_t;


void * sstub_init(sstub_cb_args_t *cb_args);
void * sstub_init_2(sstub_cb_args_t *cb_args, int bind_only_locally);
void * sstub_init_ext(sstub_cb_args_t *cb_args, 
						int bind_only_locally,
						int auth_required);
void  sstub_listen(void * cookie);
int sstub_send_obj(void *cookie, obj_data_t *obj, int vnum, int complete);
int sstub_get_partial(void *cookie, obj_data_t **obj);
int sstub_flush_objs(void *cookie, int vnum);
int sstub_send_log(void *cookie, char *buf, int len);
int sstub_wleaf_response(void *cookie, int err, int32_t opid);
int sstub_lleaf_response(void *cookie, int err, int num_ents,
                         dctl_entry_t *data, int32_t opid);
int sstub_lnode_response(void *cookie, int err, int num_ents,
                         dctl_entry_t *data, int32_t opid);
float sstub_get_drate(void *cookie);
int sstub_queued_objects(void *cookie);
int sstub_get_obj(void *cookie, sig_val_t *sig);
void sstub_get_conn_info(void *cookie, session_info_t *sinfo);

#endif /* !_LIB_SSTUB_H_ */

