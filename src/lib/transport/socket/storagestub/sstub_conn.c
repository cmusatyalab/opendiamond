/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "ring.h"
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"





/*
 * This is the loop that handles the socket communications for each
 * of the different "connectons" to the disk that exist.
 */

void *
connection_main(listener_state_t *lstate, int conn)
{
	cstate_t *		cstate;
	struct timeval 		to;
	int			max_fd;
	int			err;


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
			cstate->flags &=~CSTATE_SHUTTING_DOWN;
			cstate->flags &=~CSTATE_ALLOCATED;
			pthread_mutex_unlock(&cstate->cmutex);
			printf("exiting thread \n");
			exit(0);
		}

		FD_ZERO(&cstate->read_fds);
		FD_ZERO(&cstate->write_fds);
		FD_ZERO(&cstate->except_fds);

		FD_SET(cstate->control_fd,  &cstate->read_fds);
		FD_SET(cstate->data_fd,  &cstate->read_fds);
		FD_SET(cstate->log_fd,  &cstate->read_fds);

		FD_SET(cstate->control_fd,  &cstate->except_fds);
		FD_SET(cstate->data_fd,  &cstate->except_fds);
		FD_SET(cstate->log_fd,  &cstate->except_fds);


		pthread_mutex_lock(&cstate->cmutex);
		if (cstate->flags & CSTATE_CONTROL_DATA) {
			FD_SET(cstate->control_fd,  &cstate->write_fds);
		}
		if ((cstate->flags & CSTATE_OBJ_DATA) && 
			(cstate->cc_credits > 0)) {
			FD_SET(cstate->data_fd,  &cstate->write_fds);
		}
		if (cstate->flags & CSTATE_LOG_DATA) {
			FD_SET(cstate->log_fd,  &cstate->write_fds);
		}
		pthread_mutex_unlock(&cstate->cmutex);

		to.tv_sec = 0;
		to.tv_usec = 1000;

		/*
		 * Sleep on the set of sockets to see if anything
		 * interesting has happened.
		 */
		err = select(max_fd, &cstate->read_fds, 
				&cstate->write_fds, 
				&cstate->except_fds,  &to);

		if (err == -1) {
			/* XXX log */
            printf("XXX select %d \n", errno);
			perror("XXX select failed ");
			exit(1);
		}

		/* 
		 * If err > 0 then there are some objects
		 * that have data.
		 */
		if (err > 0) {
            /* handle reads on the sockets */
			if (FD_ISSET(cstate->control_fd, &cstate->read_fds)) {
				sstub_read_control(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &cstate->read_fds)) {
				sstub_read_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->log_fd, &cstate->read_fds)) {
				sstub_read_log(lstate, cstate);
			}

            /* handle the exception conditions on the socket */
			if (FD_ISSET(cstate->control_fd, &cstate->except_fds)) {
				sstub_except_control(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &cstate->except_fds)) {
				sstub_except_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->log_fd, &cstate->except_fds)) {
				sstub_except_log(lstate, cstate);
			}

            /* handle writes on the sockets */
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



