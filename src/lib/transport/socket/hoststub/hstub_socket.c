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
#include <stdlib.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "hstub_impl.h"
#include "ports.h"
#include "rpc_client_content_client.h"


static void disconnect_cb(void *conn_data, enum mrpc_disc_reason reason)
{
    conn_info_t *cinfo = (conn_info_t *)conn_data;

    pthread_mutex_lock(&cinfo->mutex);
    cinfo->flags |= CINFO_DOWN;
    pthread_mutex_unlock(&cinfo->mutex);
}

static struct mrpc_conn_set *mrpc_cset;
static int init_minirpc(void)
{
	if (mrpc_cset) return 0;

	if (mrpc_conn_set_create(&mrpc_cset, rpc_client_content_client, NULL)) {
		log_message(LOGT_NET, LOGL_ERR,
			    "mrpc_conn_set_create failed");
		return -1;
	}
	if (mrpc_set_disconnect_func(mrpc_cset, disconnect_cb)) {
		log_message(LOGT_NET, LOGL_ERR,
			    "mrpc_set_disconnect_func failed");
		mrpc_conn_set_unref(mrpc_cset);
		return -1;
	}
	if (mrpc_start_dispatch_thread(mrpc_cset)) {
		log_message(LOGT_NET, LOGL_ERR,
			    "mrpc_start_dispatch_thread failed");
		mrpc_conn_set_unref(mrpc_cset);
		return -1;
	}
	return 0;
}


/*
 * set a socket to non-blocking
 */
static void
socket_non_block(int fd)
{
	int flags, err;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: issue fcntl");
		return;
	}
	err = fcntl(fd, F_SETFL, (flags | O_NONBLOCK));
	if (err == -1) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: failed to set fcntl");
		return;
	}
}


static int
create_connection(const char *host, sig_val_t *session_nonce)
{
	const char *port = diamond_get_control_port();
	int sockfd, err, size;
	struct addrinfo *ai, *addrs, hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(host, port, &hints, &addrs);
	if (err) {
		log_message(LOGT_NET, LOGL_ERR,
			    "%s: Failed to resolve %s: %s\n",
			    __FUNCTION__, host, gai_strerror(err));
		return -1;
	}

	ai = addrs;
try_next:
	/* create TCP socket */
	sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sockfd < 0) {
		log_message(LOGT_NET, LOGL_ERR,
			    "%s: Failed create socket for %s: %s\n",
			    __FUNCTION__, host, strerror(errno));
		goto err_out;
	}

	/* make connection */
	err = connect(sockfd, ai->ai_addr, ai->ai_addrlen);
	if (err) {
		/* failed to connect, try next address */
		close(sockfd);
		ai = ai->ai_next;
		if (ai) goto try_next;

		log_message(LOGT_NET, LOGL_ERR,
			    "%s: Failed connect to %s: %s\n",
			    __FUNCTION__, host, strerror(errno));
		sockfd = -1;
		goto err_out;
	}

	/* Write the 32-bit session_cookie to the server. */
	size = writen(sockfd, session_nonce, sizeof(*session_nonce));
	if (size < 0) {
		log_message(LOGT_NET, LOGL_ERR,
			    "%s: Failed writing session_nonce to %s",
			    __FUNCTION__, host);
		close(sockfd);
		sockfd = -1;
		goto err_out;
	}

	/* Read 32-bit cookie from server. */
	size = readn(sockfd, session_nonce, sizeof(*session_nonce));
	if (size < 0) {
		log_message(LOGT_NET, LOGL_ERR,
			    "%s: Failed reading session_nonce",
			    __FUNCTION__);
		close(sockfd);
		sockfd = -1;
		goto err_out;
	}
err_out:
	freeaddrinfo(addrs);
	return sockfd;
}

/*
 * Create and establish a socket with the other
 * side.
 */
int
hstub_establish_connection(conn_info_t *cinfo, const char *host)
{
	struct addrinfo *ai, hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	int control_fd;
	int err;

	if (!mrpc_cset && init_minirpc())
		return -1;

	/* applications expect the device's ipv4 address filled in the
	 * device characteristics return value. */
	err = getaddrinfo(host, NULL, &hints, &ai);
	if (!err && ai) {
		cinfo->ipv4addr =
		    ((struct sockaddr_in *)ai->ai_addr)->sin_addr.s_addr;
		freeaddrinfo(ai);
	} else
		cinfo->ipv4addr = INADDR_NONE;

	/* open the control socket and receive a cookie */
	cinfo->data_fd = -1;
	memset(&cinfo->session_nonce, 0, sizeof(cinfo->session_nonce));
	control_fd = create_connection(host, &cinfo->session_nonce);
	if (control_fd < 0 ) {
		log_message(LOGT_NET, LOGL_ERR,
			    "hstub: failed to initialize control connection");
		goto err_out;
	}

	/* Now we open the data socket and send the cookie on it. */
	cinfo->data_fd = create_connection(host, &cinfo->session_nonce);
	if (cinfo->data_fd < 0) {
		log_message(LOGT_NET, LOGL_ERR,
			    "hstub: connect data port failed");
		goto err_out;
	}

	socket_non_block(cinfo->data_fd);

	/*
	 * Set the state machines variables.
	 */
	cinfo->data_rx_state = DATA_RX_NO_PENDING;

	/* hand the control socket over to minirpc. */
	if (mrpc_conn_create(&cinfo->rpc_client, mrpc_cset, cinfo)) {
		log_message(LOGT_NET, LOGL_ERR, "mrpc_conn_create failed");
		goto err_out;
	}

	if (mrpc_bind_fd(cinfo->rpc_client, control_fd)) {
		log_message(LOGT_NET, LOGL_ERR, "mrpc_connect failed");
		goto err_out;
	}
	return (0);

err_out:
	if (cinfo->data_fd >= 0)
		close(cinfo->data_fd);
	if (control_fd >= 0)
		close(control_fd);
	return (ENOENT);
}
