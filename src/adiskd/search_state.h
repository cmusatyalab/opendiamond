#ifndef _SEARCH_STATE_H_
#define _SEARCH_STATE_H_


/* some of the default constants for packet processing */
#define	SSTATE_DEFAULT_OBJ_THRESH	150
#define	SSTATE_DEFAULT_BP_THRESH	10


enum split_types_t {
	SPLIT_TYPE_FIXED = 0,	/* Defined fixed ratio of work */
	SPLIT_TYPE_DYNAMIC	/* use dynamic optimization */
};


#define	SPLIT_DEFAULT_TYPE		(SPLIT_TYPE_FIXED)
#define	SPLIT_DEFAULT_RATIO		(78)
#define	SPLIT_DEFAULT_AUTO_STEP		5
#define	SPLIT_DEFAULT_PEND_LOW		200
#define	SPLIT_DEFAULT_MULT			200
#define	SPLIT_DEFAULT_PEND_HIGH		10

#define DEV_FLAG_RUNNING                0x01
#define DEV_FLAG_COMPLETE               0x02



typedef struct search_state {
    void           *comm_cookie;	/* cookie from the communication lib */
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
    uint            pend_thresh;
    uint            split_type;		/* policy for the splitting */
    uint            split_ratio;	/* amount of computation to do local */
    uint            split_mult;	/* multiplier for queue size */
    uint            split_auto_step;	/* step to increment ration by */
    uint            split_pend_low;	/* below, not enough work for host */
    uint            split_pend_high;	/* above, too much work for host */
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


#endif                          /* ifndef _SEARCH_STATE_H_ */
