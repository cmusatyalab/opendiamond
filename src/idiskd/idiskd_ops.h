/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _IDISKD_OPS_H_
#define _IDISKD_OPS_H_

#include	"id_search_priv.h"	/* XXX move this?? */
#include	"ring.h"	/* XXX move this?? */
#define	SSTATE_DEFAULT_OBJ_THRESH	50
#define	SSTATE_DEFAULT_BP_THRESH	10

#define DEV_FLAG_RUNNING                0x01
#define DEV_FLAG_COMPLETE               0x02


typedef struct search_state
{
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
	search_status_t	cur_status;
	int				cur_search_id;
	void           *dctl_cookie;
	void           *log_cookie;
	device_handle_t *       dev_list;
	ring_data_t *           log_ring;       /* data to log  */
	ring_data_t *           bg_ops; /* unprocessed objects */
	ring_data_t *           proc_ring; /* unprocessed objects */
	struct filter_data  *   bg_fdata;
	unsigned long       bg_status;
	int                 bg_credit_policy;
	int         pend_hw;    /* pending hw mark */
	int         pend_lw;    /* pending lw mark */
	device_handle_t *   last_dev;



}
search_state_t;



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



/*
 * These are the prototypes of the device operations that
 * in the file ls_device.c
 */
int dev_new_obj_cb(void *hcookie, obj_data_t *odata, int vno);
void dev_log_data_cb(void *cookie, char *data, int len, int devid);
int lookup_group_hosts(groupid_t gid, int *num_hosts,
                       uint32_t *hostids);
int device_add_gid(search_state_t *sstate, groupid_t gid, uint32_t devid);
/*
 * These are background processing functions.
 */
int bg_init(search_state_t *sc, int id);
int bg_set_searchlet(search_state_t *sc, int id, char *filter_name,
                     char *spec_name);
int bg_set_blob(search_state_t *sc, int id, char *filter_name,
                int blob_len, void *blob_data);
int bg_start_search(search_state_t *sc, int id);
int bg_stop_search(search_state_t *sc, int id);


int log_start(search_state_t *sc);


int dctl_start(search_state_t *sc);

gid_map_t *read_gid_map(char *mapfile);

#endif                          /* ifndef _IDISKD_OPS_H_ */
