/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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


static char const cvsid[] =
    "$Header$";

/*
 * XXX constant config 
 */
#define		POLL_SECS	0
#define		POLL_USECS	200000

static void
request_chars(sdevice_state_t * dev)
{
	int             err;
	control_header_t *cheader;

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		 log_message(LOGT_NET, LOGL_ERR, 
		     "control message: malloc failed");
		return;
	}

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_GET_CHAR);
	cheader->data_len = htonl(0);

	err = ring_enq(dev->device_ops, (void *) cheader);
	if (err) {
		 log_message(LOGT_NET, LOGL_ERR, 
		     "control message: queue overflow");
		free(cheader);
		return;
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return;
}

static void
request_stats(sdevice_state_t * dev)
{
	int             err;
	control_header_t *cheader;

	cheader = (control_header_t *) malloc(sizeof(*cheader));
	if (cheader == NULL) {
		 log_message(LOGT_NET, LOGL_ERR, 
		     "control message: malloc failed");
		return;
	}

	cheader->generation_number = htonl(0);
	cheader->command = htonl(CNTL_CMD_GET_STATS);
	cheader->data_len = htonl(0);

	err = ring_enq(dev->device_ops, (void *) cheader);
	if (err) {
		 log_message(LOGT_NET, LOGL_ERR, 
		     "control message: queue overflow");
		free(cheader);
		return;
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return;
}

void
hstub_conn_down(sdevice_state_t * dev)
{
	/* callback to mark the search done */
	(*dev->hstub_conn_down_cb) (dev->hcookie, dev->ver_no);

	/* set the flag */
	dev->con_data.flags |= CINFO_DOWN;
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
			pthread_exit(0);
		}

		gettimeofday(&this_time, &tz);

		/*
		 * periodically prove send device statistics and
		 * device characteristic probes.
		 */
		if (((this_time.tv_sec == next_time.tv_sec) &&
	     	    (this_time.tv_usec >= next_time.tv_usec)) ||
		    (this_time.tv_sec > next_time.tv_sec)) {

			request_chars(dev);
			request_stats(dev);

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

		FD_SET(cinfo->control_fd, &cinfo->read_fds);

		if (!(cinfo->flags & CINFO_BLOCK_OBJ)) {
			FD_SET(cinfo->data_fd, &cinfo->read_fds);
		}

		if (cinfo->flags & CINFO_PENDING_CONTROL) {
			FD_SET(cinfo->control_fd, &cinfo->write_fds);
		}
		if (cinfo->flags & CINFO_PENDING_CREDIT) {
			FD_SET(cinfo->data_fd, &cinfo->write_fds);
		}

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
			if (FD_ISSET(cinfo->control_fd, &cinfo->read_fds)) {
				hstub_read_cntrl(dev);
			}
			if (FD_ISSET(cinfo->data_fd, &cinfo->read_fds)) {
				hstub_read_data(dev);
			}
			if (FD_ISSET(cinfo->control_fd, &cinfo->except_fds)) {
				hstub_except_cntrl(dev);
			}
			if (FD_ISSET(cinfo->data_fd, &cinfo->except_fds)) {
				hstub_except_data(dev);
			}
			if (FD_ISSET(cinfo->control_fd, &cinfo->write_fds)) {
				hstub_write_cntrl(dev);
			}
			if (FD_ISSET(cinfo->data_fd, &cinfo->write_fds)) {
				hstub_write_data(dev);
			}
		}

	}
}
