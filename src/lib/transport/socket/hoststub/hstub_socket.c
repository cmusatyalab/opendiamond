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
#include <sys/stat.h>
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
#include <assert.h>
#include "ring.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "hstub_impl.h"

/* set a socket to non-blocking */
static void
socket_non_block(int fd)
{
	int flags, err;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		/* XXX */
		printf("get flags failed \n");
		exit(1);
	}
	err = fcntl(fd, F_SETFL, (flags | O_NONBLOCK));
	if (err == -1) {
		printf("set flags failed \n");
		exit(1);
	}
}


/*
 * Create and establish a socket with the other
 * side.
 */
int
hstub_establish_connection(conn_info_t *cinfo, uint32_t devid)
{
	struct protoent	*	pent;
	struct sockaddr_in	sa;
	int			err;
	size_t			size;

	pent = getprotobyname("tcp");
	if (pent == NULL) {
		/* XXX log error */
		return(ENOENT);
	}

	cinfo->control_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);
	/* XXX err ?? */

	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short) CONTROL_PORT);
	sa.sin_addr.s_addr = devid;

	/* save the device id for later use */
	cinfo->dev_id = devid;

	err = connect(cinfo->control_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (err) {
		/* XXX log error */
		printf("failed to connected \n");
		return(ENOENT);
	}

	size = read(cinfo->control_fd, (char *)&cinfo->con_cookie, 
			sizeof(cinfo->con_cookie));
	if (size == -1) {
		/* XXX */
		printf("XXX failed to read from cntrl info \n");
		return(ENOENT);
	}

	/* XXX */
	socket_non_block(cinfo->control_fd);

	/*
	 * Now we open the data socket and send the cookie on it.
	 */

	cinfo->data_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);

	/* we reuse the sockaddr, just change the port number */
	sa.sin_port = htons((unsigned short) DATA_PORT);

	err = connect(cinfo->data_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (err) {
		/* XXX log error */
		printf("connect to data  \n");
		return(ENOENT);
	}

	/* write the cookie into the fd */
	size = send(cinfo->data_fd, (char *)&cinfo->con_cookie, 
			sizeof(cinfo->con_cookie), 0);
	if (size == -1) {
		/* XXX */
		printf("XXX failed to write data cookie \n");
		close(cinfo->con_cookie);
		return(ENOENT);
	}

	socket_non_block(cinfo->data_fd);

	/*
	 * Now we open the log socket and send the cookie on it.
	 */

	cinfo->log_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);

	/* we reuse the sockaddr, just change the port number */
	sa.sin_port = htons((unsigned short) LOG_PORT);

	err = connect(cinfo->log_fd, (struct sockaddr *)&sa, sizeof(sa));
	if (err) {
		/* XXX log error */
		printf("failed to connect to log \n");
		return(ENOENT);
	}

	/* write the cookie into the fd */
	size = write(cinfo->log_fd, (char *)&cinfo->con_cookie, 
			sizeof(cinfo->con_cookie));
	if (size == -1) {
		/* XXX */
		printf("XXX failed to write data cookie \n");
		close(cinfo->control_fd);
		close(cinfo->data_fd);
		return(ENOENT);
	}
	/* XXX */
	socket_non_block(cinfo->log_fd);

	/*
	 * Set the state machines variables.
	 */
	cinfo->control_state = CONTROL_TX_NO_PENDING;
	cinfo->control_rx_state = CONTROL_RX_NO_PENDING;
	cinfo->data_rx_state = DATA_RX_NO_PENDING;
	cinfo->log_rx_state = LOG_RX_NO_PENDING;

	return(0);
}

