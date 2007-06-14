/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
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

#ifndef _SEARCH_STATE_H_
#define _SEARCH_STATE_H_


/*
 * some of the default constants for packet processing 
 */
#define	SSTATE_DEFAULT_PEND_MAX	30


/* do we try to continue evaluating objects when stalled */ 
#define	SSTATE_DEFAULT_WORKAHEAD	1

/* name to use for logs */
#define LOG_PREFIX 	"adiskd"

enum split_types_t {
	SPLIT_TYPE_FIXED = 0,	/* Defined fixed ratio of work */
	SPLIT_TYPE_DYNAMIC	/* use dynamic optimization */
};


#define	SPLIT_DEFAULT_BP_THRESH	15
#define	SPLIT_DEFAULT_TYPE		(SPLIT_TYPE_FIXED)
#define	SPLIT_DEFAULT_RATIO		(100)
#define	SPLIT_DEFAULT_AUTO_STEP		5
#define	SPLIT_DEFAULT_PEND_LOW		200
#define	SPLIT_DEFAULT_MULT		20
#define	SPLIT_DEFAULT_PEND_HIGH		10

#define DEV_FLAG_RUNNING                0x01
#define DEV_FLAG_COMPLETE               0x02



typedef struct search_state {
	void           *comm_cookie;	/* cookie from the communication lib */
	pthread_t       thread_id;
	unsigned int    flags;
	struct odisk_state *ostate;
	struct ceval_state *cstate;
	session_info_t		cinfo;			/* used for session id */
	int             ver_no;			/* id of current search */
	ring_data_t    *control_ops;
	pthread_mutex_t log_mutex;
	pthread_cond_t  log_cond;
	pthread_t       bypass_id;
	filter_data_t  *fdata;
	uint            obj_total;
	uint            obj_processed;		/* really objects read */
	uint            obj_dropped;
	uint            obj_passed;
	uint            obj_skipped;
	uint		obj_bg_processed;	
	uint		obj_bg_dropped;
	uint		obj_bg_passed;
	uint            network_stalls;
	uint            tx_full_stalls;
	uint            tx_idles;
	uint            pend_objs;
	float           pend_compute;
	uint            pend_max;
	uint            work_ahead;	/* do we work ahead for caching */
	uint            split_type;	/* policy for the splitting */
	uint            split_ratio;	/* amount of computation to do local */
	uint            split_mult;	/* multiplier for queue size */
	uint            split_auto_step;	/* step to increment ration
						 * by */
	uint            split_bp_thresh;	/* below, not enough work for 
						 * host */
	uint            avg_int_ratio;	/* average ratio for this run */
	uint            smoothed_int_ratio;	/* integer smoothed ratio */
	float           smoothed_ratio;	/* smoothed value */
	uint            old_proc;	/* last number run */
	float           avg_ratio;	/* floating point avg ratio */
	void           *dctl_cookie;
	void           *log_cookie;
	unsigned char  *sig;
	filter_exec_mode_t	exec_mode;  /* filter execution mode */
	user_state_t	user_state; 
} search_state_t;



/*
 * Function prototypes for the search functions.
 */

int             search_new_conn(void *cookie, void **app_cookie);
int             search_close_conn(void *app_cookie);
int             search_start(void *app_cookie, int gen_num);
int             search_stop(void *app_cookie, int gen_num, host_stats_t *hs);
int             search_set_spec(void *app_cookie, int gen_num,
		    sig_val_t *spec_sig);
int             search_set_obj(void *app_cookie, int gen_num,
		    sig_val_t *obj_sig);
int             search_set_list(void *app_cookie, int gen_num);
int             search_term(void *app_cookie, int gen_num);
dev_stats_t *   search_get_stats(void *app_cookie, int gen_num);
int             search_release_obj(void *app_cookie, obj_data_t * obj);
int             search_get_char(void *app_cookie, int gen_num);
int             search_log_done(void *app_cookie, char *buf, int len);
int             search_setlog(void *app_cookie, uint32_t level, uint32_t src);
dctl_rleaf_t *  search_read_leaf(void *app_cookie, char *path, int32_t opid);
int             search_write_leaf(void *app_cookie, char *path, int len,
				  char *data, int32_t opid);
dctl_lnode_t *  search_list_nodes(void *app_cookie, char *path, int32_t opid);
int             search_list_leafs(void *app_cookie, char *path, int32_t opid);
int             search_set_gid(void *app_cookie, int gen, groupid_t gid);
int             search_clear_gids(void *app_cookie, int gen);
int             search_set_blob(void *app_cookie, int gen, char *name,
				int blob_len, void *blob_data);
int		search_set_exec_mode(void *app_cookie, uint32_t mode);
int		search_set_user_state(void *app_cookie, uint32_t state);

void		start_background();

#endif				/* ifndef _SEARCH_STATE_H_ */
