/*
 *      Diamond
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"


static char const cvsid[] =
    "$Header$";



/*
 * This is the loop that handles the socket communications for each
 * of the different "connectons" to the disk that exist.
 */

void           *
connection_main(listener_state_t * lstate, int conn)
{
	cstate_t       *cstate;
	struct timeval  to;
	int             max_fd;
	int             err;


	cstate = &lstate->conns[conn];
	/*
	 * Compute the max fd for the set of file
	 * descriptors that we care about.
	 */
	max_fd = cstate->control_fd;
	if (cstate->data_fd > max_fd) {
		max_fd = cstate->data_fd;
	}
	if (cstate->log_fd > max_fd) {
		max_fd = cstate->log_fd;
	}
	max_fd += 1;

	cstate->lstate = lstate;



	while (1) {
		if (cstate->flags & CSTATE_SHUTTING_DOWN) {
			pthread_mutex_lock(&cstate->cmutex);
			cstate->flags &= ~CSTATE_SHUTTING_DOWN;
			cstate->flags &= ~CSTATE_ALLOCATED;
			pthread_mutex_unlock(&cstate->cmutex);
			printf("exiting thread \n");
			exit(0);
		}

		FD_ZERO(&cstate->read_fds);
		FD_ZERO(&cstate->write_fds);
		FD_ZERO(&cstate->except_fds);

		FD_SET(cstate->control_fd, &cstate->read_fds);
		FD_SET(cstate->data_fd, &cstate->read_fds);
		FD_SET(cstate->log_fd, &cstate->read_fds);

		FD_SET(cstate->control_fd, &cstate->except_fds);
		FD_SET(cstate->data_fd, &cstate->except_fds);
		FD_SET(cstate->log_fd, &cstate->except_fds);


		pthread_mutex_lock(&cstate->cmutex);
		if (cstate->flags & CSTATE_CONTROL_DATA) {
			FD_SET(cstate->control_fd, &cstate->write_fds);
		}
		if ((cstate->flags & CSTATE_OBJ_DATA) &&
		    (cstate->cc_credits > 0)) {
			FD_SET(cstate->data_fd, &cstate->write_fds);
		}
		if (cstate->cc_credits == 0) {
			// printf("block on no credits \n");
			// XXX stats
		}
		if (cstate->flags & CSTATE_LOG_DATA) {
			FD_SET(cstate->log_fd, &cstate->write_fds);
		}
		pthread_mutex_unlock(&cstate->cmutex);

		to.tv_sec = 0;
		to.tv_usec = 1000;

		/*
		 * Sleep on the set of sockets to see if anything
		 * interesting has happened.
		 */
		err = select(max_fd, &cstate->read_fds,
			     &cstate->write_fds, &cstate->except_fds, &to);

		if (err == -1) {
			/*
			 * XXX log 
			 */
			printf("XXX select %d \n", errno);
			perror("XXX select failed ");
			exit(1);
		}

		/*
		 * If err > 0 then there are some objects
		 * that have data.
		 */
		if (err > 0) {
			/*
			 * handle reads on the sockets 
			 */
			if (FD_ISSET(cstate->control_fd, &cstate->read_fds)) {
				sstub_read_control(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &cstate->read_fds)) {
				sstub_read_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->log_fd, &cstate->read_fds)) {
				sstub_read_log(lstate, cstate);
			}

			/*
			 * handle the exception conditions on the socket 
			 */
			if (FD_ISSET(cstate->control_fd, &cstate->except_fds)) {
				sstub_except_control(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &cstate->except_fds)) {
				sstub_except_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->log_fd, &cstate->except_fds)) {
				sstub_except_log(lstate, cstate);
			}

			/*
			 * handle writes on the sockets 
			 */
			if (FD_ISSET(cstate->control_fd, &cstate->write_fds)) {
				sstub_write_control(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &cstate->write_fds)) {
				sstub_write_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->log_fd, &cstate->write_fds)) {
				sstub_write_log(lstate, cstate);
			}
		}
	}
}
