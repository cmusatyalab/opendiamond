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
} sstub_cb_args_t;



extern void * sstub_init(sstub_cb_args_t *cb_args);
extern void  sstub_listen(void * cookie, int fork);
extern int sstub_send_stats(void *cookie, dev_stats_t *dstats, int len);
extern int sstub_send_dev_char(void *cookie, device_char_t *dchar);
extern int sstub_send_obj(void *cookie, obj_data_t *obj, int vnum);
extern int sstub_send_log(void *cookie, char *buf, int len);
extern int sstub_wleaf_response(void *cookie, int err, int32_t opid);
extern int sstub_rleaf_response(void *cookie, int err, dctl_data_type_t dtype,
                int len, char *data, int32_t opid);
extern int sstub_lleaf_response(void *cookie, int err, int num_ents,
                dctl_entry_t *data, int32_t opid);
extern int sstub_lnode_response(void *cookie, int err, int num_ents,
                dctl_entry_t *data, int32_t opid);

#endif /* !_LIB_SSTUB_H_ */

