/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LIB_OCACHE_H_
#define	_LIB_OCACHE_H_ 	1

#include "lib_filterexec.h"
#include "obj_attr.h"

#ifdef	__cplusplus
extern "C"
{
#endif

#define ATTR_ENTRY_NUM  200

struct ocache_state;

typedef struct {
	unsigned int  	name_len;
	char		the_attr_name[MAX_ATTR_NAME];
	sig_val_t  	attr_sig;
} cache_attr_entry;

typedef struct {
	unsigned int entry_num;
	cache_attr_entry **entry_data;
} cache_attr_set;

struct cache_obj_s {
	sig_val_t		id_sig;
	sig_val_t		iattr_sig;
	int			result;
	unsigned short		eval_count; //how many times this filter is evaluated
	unsigned short		aeval_count; //how many times this filter is evaluated
	unsigned short		hit_count; //how many times this filter is evaluated
	unsigned short		ahit_count; //how many times this filter is evaluated
	cache_attr_set		iattr;
	cache_attr_set		oattr;
	query_info_t		qid;		// query that created entry
	filter_exec_mode_t	exec_mode;  // exec mode when entry created
	struct cache_obj_s	*next;
};

struct cache_init_obj_s {
	sig_val_t		id_sig;
	cache_attr_set    		attr;
	struct cache_init_obj_s	*next;
};

typedef void (*stats_drop)(void *cookie);
typedef void (*stats_process)(void *cookie);

struct ceval_state;

typedef struct ceval_state {
	pthread_t       ceval_thread_id;   // thread for cache table
	filter_data_t * fdata;
	odisk_state_t * odisk;
	void * cookie;
	stats_drop stats_drop_fn;
	stats_drop stats_process_fn;
	query_info_t	*qinfo;			// state for current search
} ceval_state_t;

typedef struct cache_obj_s cache_obj;
typedef struct cache_init_obj_s cache_init_obj;

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

typedef struct {
	void *			cache_table;
} cache_start_entry;

typedef struct {
	int			type;
	sig_val_t		id_sig;
	union {
		cache_start_entry	start;
		cache_attr_entry	iattr;		/*add input attr*/
		cache_attr_entry	oattr;		/*add output attr*/
		int			result;		/*end*/
	} u;
	query_info_t		qid;		// search that created entry
	filter_exec_mode_t  exec_mode;  // mode when entry was created
} cache_ring_entry;

typedef struct {
	attr_record_t*	arec;
	obj_data_t *	obj;
} cache_attr_t;

typedef struct {
	int				type;
	sig_val_t			id_sig;
	union {
		char            *file_name;     /* the file name to cache oattr */
		cache_attr_t		oattr;		/*add output attr*/
		sig_val_t		iattr_sig;
		sig_val_t 		fsig; /* filter signature */
	} u;
} oattr_ring_entry;

int digest_cal(filter_data_t *fdata, char *fn_name, int numarg, 
	char **filt_args, int blob_len, void *blob, sig_val_t * signature);

int cache_get_init_attrs(sig_val_t * id_sig, cache_attr_set * change_attr);

void cache_set_init_attrs(sig_val_t * id_sig, obj_attr_t *init_attr);

int cache_lookup(sig_val_t *id_sig, sig_val_t *fsig, void *fcache_table, 
	cache_attr_set *change_attr, int *err, cache_attr_set **oattr_set, 
	sig_val_t *iattr_sig, query_info_t *qinfo);

int cache_lookup2(sig_val_t *id_sig, sig_val_t *fsig, void *fcache_table, 
	cache_attr_set *change_attr, int *conf, cache_attr_set **oattr_set, 
	int *oattr_flag, int flag);

int ocache_init(char *path_name);
int ocache_start();
int ocache_stop(char *path_name);
int ocache_stop_search(sig_val_t *fsig);
int ocache_wait_finish();
int ocache_read_file(char *disk_path, sig_val_t *fsig, 
	void **fcache_table, struct timeval *atime);
int ocache_add_start(char *fhandle, sig_val_t * id_sig, void *cache_table, 
	sig_val_t *fsig);

int ocache_add_end(char *fhandle, sig_val_t * id_sig, int conf, 
				   query_info_t *qid, filter_exec_mode_t exec_mode);

int combine_attr_set(cache_attr_set *attr1, cache_attr_set *attr2);

int ceval_init_search(filter_data_t * fdata, query_info_t *qinfo,
						struct ceval_state *cstate);

int ceval_init(struct ceval_state **cstate, odisk_state_t *odisk, 
	void *cookie, stats_drop stats_drop_fn, 
	stats_process stats_process_fn);

int ceval_start(filter_data_t * fdata);
int ceval_stop(filter_data_t * fdata);

int ceval_filters1(char * obj_name, filter_data_t * fdata, void *cookie);
int ceval_filters2(obj_data_t * obj_handle, filter_data_t * fdata, 
		int force_eval, filter_exec_mode_t mode, query_info_t *qinfo,
		void *cookie, int (*continue_cb)(void *cookie));

void ceval_inject_names(char **nl, int nents);



#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_OCACHE_H */

