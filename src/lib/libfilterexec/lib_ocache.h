/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LIB_OCACHE_H_
#define	_LIB_OCACHE_H_ 	1

#include "lib_filter.h"
#include "lib_filterexec.h"
#include "obj_attr.h"

#ifdef	__cplusplus
extern "C"
{
#endif

#define ATTR_ENTRY_NUM  200

typedef void (*stats_drop)(void *cookie);
typedef void (*stats_process)(void *cookie);

typedef struct ceval_state {
	pthread_t       ceval_thread_id;   // thread for cache table
	filter_data_t * fdata;
	odisk_state_t * odisk;
	void * cookie;
	stats_drop stats_drop_fn;
	stats_drop stats_process_fn;
	query_info_t	*qinfo;			// state for current search
} ceval_state_t;

typedef struct {
	void *		cache_table;
	time_t 		mtime;
	sig_val_t 	fsig;
	struct timeval 	atime;
	int 		running;
} fcache_t;

#define		INSERT_START	0
#define		INSERT_IATTR	1
#define		INSERT_OATTR	2
#define		INSERT_END	3

int digest_cal(filter_data_t *fdata, char *fn_name, int numarg, 
	char **filt_args, int blob_len, void *blob, sig_val_t * signature);


void cache_set_init_attrs(sig_val_t * id_sig, obj_attr_t *init_attr);
int cache_get_init_attrs(query_info_t *qid, sig_val_t *idsig);

int cache_lookup(sig_val_t *id_sig, sig_val_t *fsig, query_info_t *qid,
		 int *err, int64_t *cache_entry);

void cache_combine_attr_set(query_info_t *qid, int64_t cache_entry);


int ocache_init(char *path_name);
int ocache_start(void);
int ocache_stop(char *path_name);
int ocache_stop_search(sig_val_t *fsig);
int ocache_wait_finish(void);
int ocache_read_file(char *disk_path, sig_val_t *fsig, 
		     void **fcache_table, struct timeval *atime);

int ocache_add_start(lf_obj_handle_t ohandle, sig_val_t *fsig);
int ocache_add_end(lf_obj_handle_t ohandle, sig_val_t *fsig, int conf,
		   query_info_t *qid, filter_exec_mode_t exec_mode);

int ceval_init_search(filter_data_t * fdata, query_info_t *qinfo,
		      struct ceval_state *cstate);

int ceval_init(struct ceval_state **cstate, odisk_state_t *odisk, 
	void *cookie, stats_drop stats_drop_fn, 
	stats_process stats_process_fn);

int ceval_start(filter_data_t * fdata);
int ceval_stop(filter_data_t * fdata);

int ceval_filters2(obj_data_t * obj_handle, filter_data_t * fdata, 
		   int force_eval, double *elapsed,
		   filter_exec_mode_t mode, query_info_t *qinfo,
		   void *cookie, int (*continue_cb)(void *cookie));

void ceval_inject_names(char **nl, int nents);


#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_OCACHE_H */

