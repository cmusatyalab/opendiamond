
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <stdint.h>

#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_searchlet.h"
#include "attr.h"
#include "filter_priv.h"
#include "rtimer.h"
#include "rgraph.h"

/*
 * This function walks through the list of filters and resets
 * all the statistics assocaited with each one.  This is typically
 * called when a new search is started.
 */

void
fexec_clear_stats(struct filter_info *finfo)
{
	filter_info_t *	cur_filt;


	cur_filt = finfo;

	while (cur_filt != NULL) {
		cur_filt->fi_called = 0;
		cur_filt->fi_drop = 0;
		cur_filt->fi_pass = 0;
		cur_filt->fi_time_ns = 0;

		cur_filt = cur_filt->fi_next;
	}

}


int
fexec_get_stats(struct filter_info *finfo, int max, filter_stats_t *fstats)
{
	filter_info_t *	cur_filt;
	filter_stats_t * cur_stat;
	int		cur_num;


	cur_filt = finfo;
	cur_num = 0;

	/* XXX keep the handle somewhere */
	while (cur_filt != NULL) {
		/* if we are out of space return an error */
		if (cur_num > max) {
			return(-1);
		}

		cur_stat = &fstats[cur_num];

		strncpy(cur_stat->fs_name, cur_filt->fi_name, MAX_FILTER_NAME);
		cur_stat->fs_name[MAX_FILTER_NAME-1] = '\0';
		cur_stat->fs_objs_processed = cur_filt->fi_called;
		cur_stat->fs_objs_dropped = cur_filt->fi_drop;
		if (cur_filt->fi_called != 0) {
			cur_stat->fs_avg_exec_time =  
				cur_filt->fi_time_ns / cur_filt->fi_called;
		} else {
			cur_stat->fs_avg_exec_time =  0;
		}


		/* update the number counter and the cur filter pointer */
		cur_num++;	
		cur_filt = cur_filt->fi_next;
	}

	return(0);
}
