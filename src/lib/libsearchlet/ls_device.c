/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include "ring.h"
#include "rstat.h"
#include "lib_searchlet.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "filter_priv.h"	/* to read stats -RW */
#include "consts.h"


/* XXX  ret type??*/
int
dev_new_obj_cb(void *hcookie, obj_data_t *odata, int ver_no)
{

	device_handle_t *	dev;
	int			err;
	obj_info_t *		oinfo;
	dev = (device_handle_t *)hcookie;

	printf("new_object \n");
	oinfo = (obj_info_t *)malloc (sizeof(*oinfo));
	if (oinfo == NULL ) {
		printf("XXX failed oinfo malloc \n");
		exit(1);
	}
	oinfo->ver_num = ver_no; /* XXX XXX */
	oinfo->obj = odata;	
	err = ring_enq(dev->sc->unproc_ring, (void *)oinfo);
	if (err) {
			/* XXX */
		printf("ring_enq failed \n");
	}
}



int
device_statistics(device_handle_t *dev,
		  dev_stats_t *dev_stats, int *stat_len)
{

	printf("dev stats \n");
	return(ENOENT);
}


#ifdef XXX

int
device_statistics(device_state_t *dev,
		  dev_stats_t *dev_stats, int *stat_len)
{
	filter_info_t *cur_filter;
	filter_stats_t *cur_filter_stats;
	rtime_t total_obj_time = 0;

	/* check args */
	if(!dev) return EINVAL;
	if(!dev_stats) return EINVAL;
	if(!stat_len) return EINVAL;
	if(*stat_len < sizeof(dev_stats_t)) return ENOSPC;

	memset(dev_stats, 0, *stat_len);
	
	cur_filter = dev->sc->bg_froot;
	cur_filter_stats = dev_stats->ds_filter_stats;
	while(cur_filter != NULL) {
		/* aggregate device stats */
		dev_stats->ds_num_filters++;
		/* make sure we have room for this filter */
		if(*stat_len < DEV_STATS_SIZE(dev_stats->ds_num_filters)) {
			return ENOSPC;
		}
		dev_stats->ds_objs_processed += cur_filter->fi_called;
		dev_stats->ds_objs_dropped += cur_filter->fi_drop;
		dev_stats->ds_system_load = 1; /* XXX FIX-RW */
		total_obj_time += cur_filter->fi_time_ns;
		
		/* fill in this filter stats */
		strncpy(cur_filter_stats->fs_name, cur_filter->fi_name, MAX_FILTER_NAME);
		cur_filter_stats->fs_name[MAX_FILTER_NAME-1] = '\0';
		cur_filter_stats->fs_objs_processed = cur_filter->fi_called;
		cur_filter_stats->fs_objs_dropped = cur_filter->fi_drop;
		cur_filter_stats->fs_avg_exec_time =  
			cur_filter->fi_time_ns / cur_filter->fi_called;

		cur_filter_stats++;		
		cur_filter = cur_filter->fi_next;
	}

	dev_stats->ds_avg_obj_time = total_obj_time / dev_stats->ds_objs_processed;
	/* set number of bytes used */
	*stat_len = DEV_STATS_SIZE(dev_stats->ds_num_filters);


	return 0;
}
#endif
