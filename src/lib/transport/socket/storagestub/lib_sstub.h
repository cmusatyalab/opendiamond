/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
typedef	int (*sstub_stop_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_set_searchlet_fn)(void *app_cookie, int gen_num, 
		char *spec, char *filter);
typedef	int (*sstub_set_list_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_terminate_fn)(void *app_cookie, int gen_num);
typedef	void (*sstub_getstats_fn)(void *app_cookie, int gen_num);
typedef	int (*sstub_release_obj_fn)(void *app_cookie, obj_data_t * obj);
typedef	int (*sstub_get_devchar_fn)(void *app_cookie, int gen_num);
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


typedef struct {
	sstub_new_conn_fn 	    new_conn_cb;
	sstub_close_conn_fn 	close_conn_cb;
	sstub_start_fn 		    start_cb;
	sstub_stop_fn 		    stop_cb;
	sstub_set_searchlet_fn	set_searchlet_cb;
	sstub_set_list_fn	    set_list_cb;
	sstub_terminate_fn	    terminate_cb;
	sstub_getstats_fn	    get_stats_cb;
	sstub_release_obj_fn	release_obj_cb;
	sstub_get_devchar_fn	get_char_cb;
	sstub_log_done_fn	    log_done_cb;
	sstub_set_log_fn	    setlog_cb;
	sstub_rleaf_fn	        rleaf_cb;
	sstub_wleaf_fn	        wleaf_cb;
	sstub_lleaf_fn	        lleaf_cb;
	sstub_lnode_fn	        lnode_cb;
	sstub_sgid_fn	        sgid_cb;
	sstub_clear_gids_fn		clear_gids_cb;
	sstub_set_blob_fn		set_blob_cb;
	sstub_set_offload_fn    set_offload_cb;
} sstub_cb_args_t;



void * sstub_init(sstub_cb_args_t *cb_args);
void  sstub_listen(void * cookie, int fork);
int sstub_send_stats(void *cookie, dev_stats_t *dstats, int len);
int sstub_send_dev_char(void *cookie, device_char_t *dchar);
int sstub_send_obj(void *cookie, obj_data_t *obj, int vnum, int complete);
int sstub_get_partial(void *cookie, obj_data_t **obj);
int sstub_flush_objs(void *cookie, int vnum);
int sstub_send_log(void *cookie, char *buf, int len);
int sstub_wleaf_response(void *cookie, int err, int32_t opid);
int sstub_rleaf_response(void *cookie, int err, dctl_data_type_t dtype,
                int len, char *data, int32_t opid);
int sstub_lleaf_response(void *cookie, int err, int num_ents,
                dctl_entry_t *data, int32_t opid);
int sstub_lnode_response(void *cookie, int err, int num_ents,
                dctl_entry_t *data, int32_t opid);
float sstub_get_drate(void *cookie);
int sstub_queued_objects(void *cookie);


#endif /* !_LIB_SSTUB_H_ */

