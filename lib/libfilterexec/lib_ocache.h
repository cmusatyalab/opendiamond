/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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

#include <time.h>

#include "lib_filter.h"
#include "lib_filterexec.h"

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

int ocache_init(char *path_name);

int ocache_start(void);

int ocache_stop(char *path_name);

int ceval_init_search(filter_data_t * fdata, query_info_t *qinfo,
		      struct ceval_state *cstate);

int ceval_init(struct ceval_state **cstate, odisk_state_t *odisk, 
	void *cookie, stats_drop stats_drop_fn, 
	stats_process stats_process_fn);

int ceval_start(filter_data_t * fdata);

int ceval_stop(filter_data_t * fdata);

pr_obj_t *ceval_filters1(char *objname, filter_data_t *fdata,
			 ceval_state_t *cstate);

int ceval_filters2(obj_data_t * obj_handle, filter_data_t * fdata, 
		   int force_eval, double *elapsed, query_info_t *qinfo);

void ceval_inject_names(char **nl, int nents);

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_OCACHE_H */

