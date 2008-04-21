/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
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

#include <netinet/in.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"

/*
 * This is the loop that handles the socket communications for each
 * of the different "connections" to the disk that exist.
 */
void
connection_main(listener_state_t * lstate, int i)
{
	cstate_t       *cstate;
	struct timeval  to;
	int		max_fd;
	int		err;
	fd_set		read_fds;
	fd_set		write_fds;
	fd_set		except_fds;

	cstate = &lstate->conns[i];
	cstate->lstate = lstate;

	if (sstub_bind_conn(cstate) != 0) {
		perror("sstub_bind_conn");
		return;
	}

	max_fd = cstate->data_fd + 1;

	while (1) {
		if (cstate->flags & CSTATE_SHUTTING_DOWN) {
			pthread_mutex_lock(&cstate->cmutex);
			cstate->flags &= ~CSTATE_SHUTTING_DOWN;
			cstate->flags &= ~CSTATE_ALLOCATED;
			pthread_mutex_unlock(&cstate->cmutex);
			printf("exiting thread \n");
			pthread_exit((void *)0);
		}

		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_ZERO(&except_fds);

		FD_SET(cstate->data_fd, &read_fds);
		FD_SET(cstate->data_fd, &except_fds);

		pthread_mutex_lock(&cstate->cmutex);

		if ((cstate->flags & CSTATE_OBJ_DATA) &&
		    (cstate->cc_credits > 0)) {
			FD_SET(cstate->data_fd, &write_fds);
		}

		pthread_mutex_unlock(&cstate->cmutex);

		to.tv_sec = 1;
		to.tv_usec = 0;

		/*
		 * Sleep on the set of sockets to see if anything
		 * interesting has happened.
		 */
		err = select(max_fd, &read_fds, &write_fds, &except_fds, &to);

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
			/* handle outgoing data on the data connection */
			if (FD_ISSET(cstate->data_fd, &read_fds))
				sstub_read_data(cstate);

			if (FD_ISSET(cstate->data_fd, &write_fds))
				sstub_write_data(cstate);

			if (FD_ISSET(cstate->data_fd, &except_fds))
				sstub_except_data(cstate);
		}
	}
}
