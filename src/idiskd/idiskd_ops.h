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

#ifndef _IDISKD_OPS_H_
#define _IDISKD_OPS_H_

#define	SSTATE_DEFAULT_OBJ_THRESH	50
#define	SSTATE_DEFAULT_BP_THRESH	10

#define DEV_FLAG_RUNNING                0x01
#define DEV_FLAG_COMPLETE               0x02


typedef struct search_state {
    void           *comm_cookie;
    pthread_t       thread_id;
    unsigned int    flags;
    struct odisk_state *ostate;
    int             ver_no;
    ring_data_t    *control_ops;
    pthread_mutex_t log_mutex;
    pthread_cond_t  log_cond;
    pthread_t       log_thread;
    filter_data_t  *fdata;
    uint            obj_total;
    uint            obj_processed;
    uint            obj_dropped;
    uint            obj_passed;
    uint            obj_skipped;
    uint            pend_objs;
	float			pend_compute;
    uint            pend_thresh;
    uint            bp_feedback;
    uint            bp_thresh;
    void           *dctl_cookie;
    void           *log_cookie;
} search_state_t;



/*
 * Function prototypes for the search functions.
 */

int             search_new_conn(void *cookie, void **app_cookie);
int             search_close_conn(void *app_cookie);
int             search_start(void *app_cookie, int gen_num);
int             search_stop(void *app_cookie, int gen_num);
int             search_set_searchlet(void *app_cookie, int gen_num,
                                     char *filter, char *spec);
int             search_set_list(void *app_cookie, int gen_num);
int             search_term(void *app_cookie, int gen_num);
void            search_get_stats(void *app_cookie, int gen_num);
int             search_release_obj(void *app_cookie, obj_data_t * obj);
int             search_get_char(void *app_cookie, int gen_num);
int             search_log_done(void *app_cookie, char *buf, int len);
int             search_setlog(void *app_cookie, uint32_t level, uint32_t src);
int             search_read_leaf(void *app_cookie, char *path, int32_t opid);
int             search_write_leaf(void *app_cookie, char *path, int len,
                                  char *data, int32_t opid);
int             search_list_nodes(void *app_cookie, char *path, int32_t opid);
int             search_list_leafs(void *app_cookie, char *path, int32_t opid);
int             search_set_gid(void *app_cookie, int gen, groupid_t gid);
int             search_clear_gids(void *app_cookie, int gen);
int             search_set_blob(void *app_cookie, int gen, char *name,
                                int blob_len, void *blob_data);
int             search_set_offload(void *app_cookie, int gen, uint64_t data);


#endif                          /* ifndef _IDISKD_OPS_H_ */
