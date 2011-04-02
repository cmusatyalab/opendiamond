/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "sys_attr.h"
#include "lib_filterexec.h"
#include "filter_priv.h"


/*
 * This function walks through the list of filters and resets
 * all the statistics assocaited with each one.  This is typically
 * called when a new search is started.
 */

void
fexec_clear_stats(filter_data_t * fdata)
{
	int             i;

	for (i = 0; i < fdata->fd_num_filters; i++) {
		fdata->fd_filters[i].fi_called = 0;
		fdata->fd_filters[i].fi_drop = 0;
		fdata->fd_filters[i].fi_pass = 0;
		fdata->fd_filters[i].fi_error = 0;
		fdata->fd_filters[i].fi_time_ns = 0;
		fdata->fd_filters[i].fi_cache_drop = 0;
		fdata->fd_filters[i].fi_cache_pass = 0;
		fdata->fd_filters[i].fi_compute = 0;
		fdata->fd_filters[i].fi_added_bytes = 0;
		fdata->fd_filters[i].fi_hits_inter_session = 0;
		fdata->fd_filters[i].fi_hits_inter_query = 0;
		fdata->fd_filters[i].fi_hits_intra_query = 0;
	}
}

/*
 * Get the statistics for each of the filters.
 */

int
fexec_get_stats(filter_data_t * fdata, int max, filter_stats_t * fstats)
{
	filter_info_t  *cur_filt;
	filter_stats_t *cur_stat;
	int             i;

	if (fdata == NULL) {
		return (-1);
	}
	/*
	 * XXX keep the handle somewhere 
	 */
	for (i = 0; i < fdata->fd_num_filters; i++) {
		cur_filt = &fdata->fd_filters[i];

		/*
		 * if we are out of space return an error 
		 */
		if (i > max) {
			return (-1);
		}

		cur_stat = &fstats[i];

		strncpy(cur_stat->fs_name, cur_filt->fi_name,
			MAX_FILTER_NAME);
		cur_stat->fs_name[MAX_FILTER_NAME - 1] = '\0';
		cur_stat->fs_objs_processed = cur_filt->fi_called;
		cur_stat->fs_objs_dropped = cur_filt->fi_drop;

		/*
		 * JIAYING 
		 */
		cur_stat->fs_objs_cache_dropped = cur_filt->fi_cache_drop;
		cur_stat->fs_objs_cache_passed = cur_filt->fi_cache_pass;
		cur_stat->fs_objs_compute = cur_filt->fi_compute;
		/*
		 * JIAYING 
		 */
		cur_stat->fs_hits_inter_session = cur_filt->fi_hits_inter_session;
	    cur_stat->fs_hits_inter_query = cur_filt->fi_hits_inter_query;
	 	cur_stat->fs_hits_intra_query = cur_filt->fi_hits_intra_query;
		
		if (cur_filt->fi_called != 0) {
			cur_stat->fs_avg_exec_time =
			    cur_filt->fi_time_ns / cur_filt->fi_called;
			// printf("filter %s was called %d times, time_ns
			// %lld\n", cur_filt->fi_name, cur_filt->fi_called,
			// cur_filt->fi_time_ns);
		} else {
			cur_stat->fs_avg_exec_time = 0;
			// printf("filter %s has 0 fi_called\n",
			// cur_filt->fi_name);
		}

	}
	return (0);
}


float
fexec_get_prate(filter_data_t * fdata)
{
	float           avg;
	if (fdata == NULL) {
		return (1.0);
	}
	avg = 1.0 / fdata->fd_avg_wall;
	return (avg);
}
