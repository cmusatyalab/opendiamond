/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
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
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_hstub.h"
#include "lib_auth.h"
#include "hstub_impl.h"
#include "rpc_client_content.h"

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
	enum clnt_stat retval;
	request_chars_return_x characteristics;

	retval = request_chars_x_2(0, &characteristics,
			       dev->con_data.tirpc_client);
	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "request_chars: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  return -1;
	}
	if(characteristics.error.service_err != DIAMOND_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "request_chars: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&characteristics.error));
	  return -1;
	}

	dev->dev_char.dc_isa = characteristics.chars.dcs_isa;
	dev->dev_char.dc_speed = characteristics.chars.dcs_isa;
	dev->dev_char.dc_mem = characteristics.chars.dcs_mem;
	dev->dev_char.dc_devid = dev->con_data.dev_id;
	
	return 0;
}


/*
 * This stores caches the statistics to answer requests
 * from users.
 */

static int
request_stats(sdevice_state_t * dev)
{
	enum clnt_stat retval;
	request_stats_return_x statistics;
	dev_stats_t    *dstats;
	int             len;
	int             num_filt;
	int             i;

	retval = request_stats_x_2(0, &statistics, dev->con_data.tirpc_client);
	if (retval != RPC_SUCCESS) {
	  log_message(LOGT_NET, LOGL_ERR, "request_stats: call sending failed");
	  log_message(LOGT_NET, LOGL_ERR, clnt_sperrno(retval));
	  return -1;
	}
	if(statistics.error.service_err != DIAMOND_SUCCESS) {
	  if((statistics.error.service_err == DIAMOND_OPERR) &&
	     (statistics.error.opcode_err == DIAMOND_OPCODE_NOSTATSAVAIL)) {
	    return 0;  // we often ask for stats when no search is running
	  }
	  log_message(LOGT_NET, LOGL_ERR, "request_stats: call servicing failed");
	  log_message(LOGT_NET, LOGL_ERR, diamond_error(&statistics.error));
	  return -1;
	}

	num_filt = statistics.stats.ds_filter_stats.ds_filter_stats_len;
	len = DEV_STATS_SIZE(num_filt);


	if (len > dev->stat_size) {
		if (dev->dstats != NULL) {
			free(dev->dstats);
		}
		dstats = (dev_stats_t *) malloc(len);
		assert(dstats != NULL);
		dev->dstats = dstats;
		dev->stat_size = len;
	} else {
		dstats = dev->dstats;
		dev->stat_size = len;
	}


	dstats->ds_objs_total = statistics.stats.ds_objs_total;
	dstats->ds_objs_processed = statistics.stats.ds_objs_processed;
	dstats->ds_objs_dropped = statistics.stats.ds_objs_dropped;
	dstats->ds_objs_nproc = statistics.stats.ds_objs_nproc;
	dstats->ds_system_load = statistics.stats.ds_system_load;
	dstats->ds_avg_obj_time = statistics.stats.ds_avg_obj_time;
	dstats->ds_num_filters = statistics.stats.ds_filter_stats.ds_filter_stats_len;

	for (i = 0; i < num_filt; i++) {
		strncpy(dstats->ds_filter_stats[i].fs_name,
			statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_name, MAX_FILTER_NAME);
		dstats->ds_filter_stats[i].fs_name[MAX_FILTER_NAME - 1] =
		    '\0';

		dstats->ds_filter_stats[i].fs_objs_processed =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_processed;

		dstats->ds_filter_stats[i].fs_objs_dropped =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_dropped;

		/*
		 * JIAYING 
		 */
		dstats->ds_filter_stats[i].fs_objs_cache_dropped =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_dropped;
		dstats->ds_filter_stats[i].fs_objs_cache_passed =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_cache_passed;
		dstats->ds_filter_stats[i].fs_objs_compute =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_objs_compute;

		dstats->ds_filter_stats[i].fs_hits_inter_session =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_session;
		dstats->ds_filter_stats[i].fs_hits_inter_query =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_inter_query;
		dstats->ds_filter_stats[i].fs_hits_intra_query =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_hits_intra_query;

		/*
		 * XXX byte order !!! 
		 */
		
		dstats->ds_filter_stats[i].fs_avg_exec_time =
		    statistics.stats.ds_filter_stats.ds_filter_stats_val[i].fs_avg_exec_time;
	}

	return 0;
}

void
hstub_conn_down(sdevice_state_t * dev)
{
	/* callback to mark the search done */
	(*dev->hstub_conn_down_cb) (dev->hcookie, dev->ver_no);

	/* set the flag */
	dev->con_data.flags |= CINFO_DOWN;
	log_message(LOGT_NET, LOGL_CRIT, 
	     "hstub_conn_down: Killing thread..\n");
	pthread_exit(0); 
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
	struct timeval  to;
	int             err;
	int             max_fd;
	struct timeval  this_time;
	struct timeval  next_time = {0, 0};
	struct timezone tz;

	dev = (sdevice_state_t *) arg;

	dctl_thread_register(dev->dctl_cookie);
	log_thread_register(dev->log_cookie);

	signal(SIGPIPE, SIG_IGN);

	/*
	 * XXX need to open comm channel with device
	 */
	cinfo = &dev->con_data;

	max_fd = cinfo->control_fd;
	if (cinfo->data_fd > max_fd) {
		max_fd = cinfo->data_fd;
	}
	max_fd += 1;

	/*
	 * This loop looks at the set of items that we need to handle.
	 * This includes the ring_queue of of outstanding operations, 
	 * as well as monitoring the sockets to see what data
	 * is available for processing.
	 */
	while (1) {

		/* if the connection has been marked down then we
		 * exit for now. 
		 * TODO: future version should possibly start over.
		 */
		if (cinfo->flags & CINFO_DOWN) {
		  log_message(LOGT_NET, LOGL_CRIT, 
			      "hstub_main: conn marked down. Killing thread..\n");			pthread_exit(0);
		}

		gettimeofday(&this_time, &tz);

		/*
		 * periodically prove send device statistics and
		 * device characteristic probes.
		 */
		if (((this_time.tv_sec == next_time.tv_sec) &&
	     	    (this_time.tv_usec >= next_time.tv_usec)) ||
		    (this_time.tv_sec > next_time.tv_sec)) {

			if((request_chars(dev) < 0) || (request_stats(dev) < 0)) {
			  log_message(LOGT_NET, LOGL_CRIT, 
				      "hstub_main: TS-RPC calls are failing. Killing thread..\n");
			  hstub_conn_down(dev); 
			}

			assert(POLL_USECS < 1000000);
			next_time.tv_sec = this_time.tv_sec + POLL_SECS;
			next_time.tv_usec = this_time.tv_usec + POLL_USECS;

			if (next_time.tv_usec >= 1000000) {
				next_time.tv_usec -= 1000000;
				next_time.tv_sec += 1;
			}
		}


		FD_ZERO(&cinfo->read_fds);
		FD_ZERO(&cinfo->write_fds);
		FD_ZERO(&cinfo->except_fds);

		if (!(cinfo->flags & CINFO_BLOCK_OBJ)) {
			FD_SET(cinfo->data_fd, &cinfo->read_fds);
		}

		if (cinfo->flags & CINFO_PENDING_CREDIT) {
			FD_SET(cinfo->data_fd, &cinfo->write_fds);
		}

		FD_SET(cinfo->data_fd, &cinfo->except_fds);

		to.tv_sec = 1;
		to.tv_usec = 0;


		err = select(max_fd, &cinfo->read_fds, &cinfo->write_fds,
			     &cinfo->except_fds, &to);

		if (err == -1) {
		 	log_message(LOGT_NET, LOGL_CRIT, 
				    "hstub_main: broken socket");
			hstub_conn_down(dev);
		}

		if (err > 0) {
			if (FD_ISSET(cinfo->data_fd, &cinfo->read_fds)) {
				hstub_read_data(dev);
			}
			if (FD_ISSET(cinfo->data_fd, &cinfo->except_fds)) {
				hstub_except_data(dev);
			}
			if (FD_ISSET(cinfo->data_fd, &cinfo->write_fds)) {
				hstub_write_data(dev);
			}
		}

	}
}
