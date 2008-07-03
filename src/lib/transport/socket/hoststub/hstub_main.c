/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_hstub.h"
#include "hstub_impl.h"

#include "rpc_client_content_client.h"

/*
 * XXX constant config 
 */
#define		POLL_SECS	0
#define		POLL_USECS	200000


/*
 * Take the device characteristics we recieved and
 * store them as part of the device state.  We will use
 * these to answer requests.  The freshness will be determined
 * by how often we go an make the requests.
 */
static int
request_chars(sdevice_state_t * dev)
{
	mrpc_status_t	retval;
	dev_char_x	*characteristics = NULL;
	int		err;

	retval = rpc_client_content_request_chars(dev->con_data.rpc_client,
						  &characteristics);

	err = rpc_postproc(__FUNCTION__, retval);
	if (err) goto err_out;

	dev->dev_char.dc_isa = characteristics->dcs_isa;
	dev->dev_char.dc_speed = characteristics->dcs_isa;
	dev->dev_char.dc_mem = characteristics->dcs_mem;

	dev->dev_char.dc_devid = dev->con_data.ipv4addr;
err_out:
	free_dev_char_x(characteristics, 1);
	return err;
}


/*
 * This stores caches the statistics to answer requests
 * from users.
 */
static int
request_stats(sdevice_state_t * dev)
{
	mrpc_status_t	retval;
	dev_stats_x	*statistics = NULL;
	dev_stats_t	*dstats;
	int		len;
	int		num_filt;
	int		i;
	int		unavail;
	int		err;

	retval = rpc_client_content_request_stats(dev->con_data.rpc_client,
						  &statistics);

	/* when filters are not loaded we get an 'error', avoid logging it */
	unavail = (retval == DIAMOND_NOSTATSAVAIL);
	if (unavail) retval = DIAMOND_SUCCESS;

	err = rpc_postproc(__FUNCTION__, retval);
	if (err || unavail) goto err_out;

	num_filt = statistics->ds_filter_stats.ds_filter_stats_len;
	len = DEV_STATS_SIZE(num_filt);

	if (len > dev->stat_size) {
		if (dev->dstats != NULL)
			free(dev->dstats);

		dstats = (dev_stats_t *) malloc(len);
		assert(dstats != NULL);
		dev->dstats = dstats;
		dev->stat_size = len;
	} else {
		dstats = dev->dstats;
		dev->stat_size = len;
	}

	dstats->ds_objs_total = statistics->ds_objs_total;
	dstats->ds_objs_processed = statistics->ds_objs_processed;
	dstats->ds_objs_dropped = statistics->ds_objs_dropped;
	dstats->ds_objs_nproc = statistics->ds_objs_nproc;
	dstats->ds_system_load = statistics->ds_system_load;
	dstats->ds_avg_obj_time = statistics->ds_avg_obj_time;
	dstats->ds_num_filters = statistics->ds_filter_stats.ds_filter_stats_len;

	for (i = 0; i < num_filt; i++) {
		strncpy(dstats->ds_filter_stats[i].fs_name,
			statistics->ds_filter_stats.ds_filter_stats_val[i].fs_name, MAX_FILTER_NAME);
		dstats->ds_filter_stats[i].fs_name[MAX_FILTER_NAME - 1] =
		    '\0';

		dstats->ds_filter_stats[i].fs_objs_processed =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_objs_processed;

		dstats->ds_filter_stats[i].fs_objs_dropped =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_objs_dropped;

		/*
		 * JIAYING
		 */
		dstats->ds_filter_stats[i].fs_objs_cache_dropped =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_dropped;
		dstats->ds_filter_stats[i].fs_objs_cache_passed =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_passed;
		dstats->ds_filter_stats[i].fs_objs_compute =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_objs_compute;

		dstats->ds_filter_stats[i].fs_hits_inter_session =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_session;
		dstats->ds_filter_stats[i].fs_hits_inter_query =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_query;
		dstats->ds_filter_stats[i].fs_hits_intra_query =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_hits_intra_query;

		/*
		 * XXX byte order !!!
		 */

		dstats->ds_filter_stats[i].fs_avg_exec_time =
		    statistics->ds_filter_stats.ds_filter_stats_val[i].fs_avg_exec_time;
	}
err_out:
	free_dev_stats_x(statistics, 1);
	return err;
}

/*
 * The main loop that the per device thread runs while
 * processing data to/from the individual devices
 */
void *
hstub_main(void *arg)
{
	sdevice_state_t *dev;
	conn_info_t    *cinfo;
	int		closed;
	int		credit;

	dev = (sdevice_state_t *) arg;

	signal(SIGPIPE, SIG_IGN);

	/*
	 * XXX need to open comm channel with device
	 */
	cinfo = &dev->con_data;

	/*
	 * This loop looks at the set of items that we need to handle.
	 * This includes the ring_queue of of outstanding operations, 
	 * as well as monitoring the sockets to see what data
	 * is available for processing.
	 */
	while (1) {
		struct timeval to = { .tv_sec = POLL_SECS, .tv_usec = POLL_USECS };

		/* if the connection has been marked down then we exit.
		 * TODO: future version should possibly start over.
		 */
		pthread_mutex_lock(&dev->con_data.mutex);
		closed = (cinfo->flags & CINFO_DOWN && cinfo->ref == 0);
		credit = (cinfo->flags & CINFO_PENDING_CREDIT);
		pthread_mutex_unlock(&dev->con_data.mutex);

		if (closed) {
			log_message(LOGT_NET, LOGL_CRIT,
				    "hstub_main: conn marked down...\n");
			break;
		}

		if (credit)
			hstub_send_credits(dev);

		/*
		 * periodically send device statistics and
		 * device characteristic probes.
		 */
		if ((request_chars(dev) < 0) ||
		    (request_stats(dev) < 0))
		{
			log_message(LOGT_NET, LOGL_CRIT,
				    "hstub_main: RPC calls are failing."
				    " Killing thread..\n");
			break;
		}
		select(0, NULL, NULL, NULL, &to);
	}

	mrpc_conn_unref(cinfo->rpc_client);
	mrpc_conn_unref(cinfo->blast_conn);
	cinfo->rpc_client = cinfo->blast_conn = NULL;

	(*dev->cb.conn_down_cb) (dev->hcookie);

	log_message(LOGT_NET, LOGL_CRIT, "hstub_main: Killing thread..\n");
	pthread_exit(0);
}

