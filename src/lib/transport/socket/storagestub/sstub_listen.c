/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
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
#include "lib_log.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "dctl_common.h"
#include "lib_sstub.h"
#include "sstub_impl.h"
#include "dctl_impl.h"

/*
 * XXX debug 
 */
#define OBJ_RING_SIZE		512
#define CONTROL_RING_SIZE	1024


/*
 * set a socket to non-blocking 
 */
static void
socket_non_block(int fd)
{
	int             flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		/*
		 * XXX 
		 */
	}
	fcntl(fd, F_SETFL, (flags | O_NONBLOCK));

}

static void
register_stats(cstate_t * cstate)
{
	dctl_register_u32(DEV_NETWORK_PATH, "obj_sent", O_RDONLY,
			  &cstate->stats_objs_tx);
	dctl_register_u64(DEV_NETWORK_PATH, "obj_tot_bytes_sent", O_RDONLY,
			  &cstate->stats_objs_total_bytes_tx);
	dctl_register_u64(DEV_NETWORK_PATH, "obj_data_bytes_sent", O_RDONLY,
			  &cstate->stats_objs_data_bytes_tx);
	dctl_register_u64(DEV_NETWORK_PATH, "obj_attr_bytes_sent", O_RDONLY,
			  &cstate->stats_objs_attr_bytes_tx);
	dctl_register_u64(DEV_NETWORK_PATH, "obj_hdr_bytes_sent", O_RDONLY,
			  &cstate->stats_objs_hdr_bytes_tx);

	dctl_register_u32(DEV_NETWORK_PATH, "control_sent", O_RDONLY,
			  &cstate->stats_control_tx);
	dctl_register_u64(DEV_NETWORK_PATH, "control_bytes_sent", O_RDONLY,
			  &cstate->stats_control_bytes_tx);
	dctl_register_u32(DEV_NETWORK_PATH, "control_recv", O_RDONLY,
			  &cstate->stats_control_rx);
	dctl_register_u64(DEV_NETWORK_PATH, "control_bytes_recv", O_RDONLY,
			  &cstate->stats_control_bytes_rx);

	dctl_register_u32(DEV_NETWORK_PATH, "attr_policy", O_RDWR,
			  &cstate->attr_policy);
	dctl_register_u32(DEV_NETWORK_PATH, "attr_ratio", O_RDWR,
			  &cstate->attr_ratio);
	dctl_register_u32(DEV_NETWORK_PATH, "cc_credits", O_RDONLY,
			  &cstate->cc_credits);
}

/*
 * This is called if the connection has been shutdown the remote
 * side or some error connection.  We notify the caller of the library
 * and clean up the state.
 */

void
shutdown_connection(cstate_t *cstate)
{
	/* Notify the "application" through the callback. */
	(*cstate->lstate->cb.close_conn_cb) (cstate->app_cookie);

	mrpc_conn_close(cstate->mrpc_conn);
	mrpc_conn_close(cstate->blast_conn);
	cstate->mrpc_conn = cstate->blast_conn = NULL;

	pthread_mutex_lock(&cstate->cmutex);
	/* if there is a control socket, close it */
	if (cstate->flags & CSTATE_CNTRL_FD) {
		close(cstate->control_fd);
		cstate->flags &= ~CSTATE_CNTRL_FD;
	}

	/* if there is a data socket, close it */
	if (cstate->flags & CSTATE_DATA_FD) {
		close(cstate->data_fd);
		cstate->flags &= ~CSTATE_DATA_FD;
	}

	if (cstate->thumbnail_set) {
		g_array_free(cstate->thumbnail_set, TRUE);
		cstate->thumbnail_set = NULL;
	}
	cstate->flags &= ~CSTATE_ALLOCATED;
	pthread_mutex_unlock(&cstate->cmutex);
}


/*
 * Create and establish a socket with the other
 * side.
 */
int
sstub_new_sock(int *fd, const char *port, int bind_only_locally)
{
	int             new_fd;
	int             err;
	int             yes = 1;

	struct addrinfo *ai, *addrs, hints = {
	    .ai_family = AF_UNSPEC,
	    .ai_socktype = SOCK_STREAM
	};

	if (!bind_only_locally)
	    hints.ai_flags |= AI_PASSIVE;

	err = getaddrinfo(NULL, port, &hints, &addrs);
	if (err) return ENOENT;

	ai = addrs;
try_next:
	if (!ai) {
		perror("Bind failed");
		freeaddrinfo(addrs);
		return ENOENT;
	}

	new_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (new_fd < 0) {
		ai = ai->ai_next;
		goto try_next;
	}

	/*
	 * set the socket options to avoid the re-use message if
	 * we have a fast restart.
	 */
	err = setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (err == -1) {
		/* XXX log */
		close(new_fd);
		ai = ai->ai_next;
		goto try_next;
	}

	err = bind(new_fd, ai->ai_addr, ai->ai_addrlen);
	if (err) {
		/* XXX log */
		close(new_fd);
		ai = ai->ai_next;
		goto try_next;
	}

	err = listen(new_fd, SOMAXCONN);
	if (err) {
		/* XXX log */
		close(new_fd);
		ai = ai->ai_next;
		goto try_next;
	}

	freeaddrinfo(addrs);

	*fd = new_fd;
	return (0);
}


/*
 * We have all the sockets endpoints open for this new connection.
 * Now we span a thread for managing this connection as well
 * using the callbacks to inform the caller that there is a new
 * connection to be serviced.
 */
static void
have_full_conn(listener_state_t * list_state, int conn)
{

	int		err;
	void		*new_cookie;
	cstate_t	*cstate;
	int		parent;

	/* set up connection info */
	cstate = &list_state->conns[conn];
	cstate->cinfo.conn_idx = conn;
	timerclear(&cstate->cinfo.connect_time);
	gettimeofday(&cstate->cinfo.connect_time, NULL);

	err = ring_init(&cstate->complete_obj_ring, OBJ_RING_SIZE);
	if (err) {
		/*
		 * XXX 
		 */
		printf("failed to init complete obj ring \n");
		return;
	}

	err = ring_init(&cstate->partial_obj_ring, OBJ_RING_SIZE);
	if (err) {
		/*
		 * XXX 
		 */
		printf("failed to init partial obj ring \n");
		return;
	}

	cstate->lstate = list_state;

	parent = (*list_state->cb.new_conn_cb) ((void *) cstate, &new_cookie);
	if (parent) {
		shutdown_connection(cstate);
		return;
	}

	/*
	 * we registered correctly, save the cookie;
	 */
	cstate->app_cookie = new_cookie;

	/*
	 * Register the statistics with dctl.  This needs to be
	 * done after the new_conn_cb()  !!!.
	 */
	register_stats(cstate);

	cstate->attr_policy = DEFAULT_NW_ATTR_POLICY;
	cstate->attr_threshold = RAND_MAX;
	cstate->attr_ratio = DEFAULT_NW_ATTR_RATIO;

	/*
	 * the main thread of this process is used
	 * for servicing the connections.
	 */
	connection_main(cstate);

	shutdown_connection(cstate);
	exit(0);
}

static const sig_val_t nullsig;

/* create a unique nonce, hashing the contents of /proc/stat on Linux has the
 * nice property that it contains various timers and interrupt counters */
static void
new_nonce(sig_val_t *sig)
{
	int fd = open("/dev/urandom", O_RDONLY);
	read(fd, sig, sizeof(*sig));
	close(fd);
}

/*
 * This accepts an incoming connection request. We accept the connection and
 * receive some data. This should tell us whether this is a new connection or
 * what connection it belongs to (if the caller did the correct handshake).
 */
static void
accept_connection(listener_state_t * list_state)
{
	struct sockaddr_storage peer;
	socklen_t	peerlen;
	int		sockfd;
	cstate_t	*conn;
	ssize_t	n;
	sig_val_t	nonce;
	int		i;

	peerlen = sizeof(peer);
	sockfd = accept(list_state->listen_fd,
			(struct sockaddr *)&peer, &peerlen);
	if (sockfd < 0) {
		/* XXX log */
		printf("XXX accept failed \n");
		return;
	}

	n = read(sockfd, &nonce, sizeof(nonce));
	if (n != sizeof(nonce)) {
		/* XXX */
		printf("failed to read cookie \n");
		close(sockfd);
		return;
	}
	if (memcmp(&nonce, &nullsig, sizeof(nonce)) == 0) {
		/* Now we allocate a new connection state and store the socket
		* associated with this. */
		for (i = 0; i < MAX_CONNS; i++) {
			conn = &list_state->conns[i];
			if (!(conn->flags & CSTATE_ALLOCATED))
				break;
		}
		if (i == MAX_CONNS) {
			/* XXX log */
			printf("XXX accept control no available state found\n");
			close(sockfd);
			return;
		}

		conn->flags |= CSTATE_ALLOCATED;
		conn->flags |= CSTATE_CNTRL_FD;
		conn->control_fd = sockfd;

		memcpy(&conn->cinfo.clientaddr, &peer, peerlen);
		conn->cinfo.clientaddr_len = peerlen;

		pthread_mutex_init(&conn->cmutex, NULL);
		new_nonce(&conn->nonce);
	}
	else {
		/* Now we find the matching connection state and associate the
		 * incoming data connection with this. */
		for (i = 0; i < MAX_CONNS; i++) {
			conn = &list_state->conns[i];
			if (conn->flags & CSTATE_ALLOCATED &&
			    memcmp(&nonce, &conn->nonce, sizeof(nonce)) == 0)
				break;
		}
		if (i == MAX_CONNS) {
			/* XXX log */
			printf("XXX accept data no matching state found\n");
			close(sockfd);
			return;
		}
		conn->flags |= CSTATE_DATA_FD;
		conn->data_fd = sockfd;

		socket_non_block(sockfd);
	}

	n = write(sockfd, &conn->nonce, sizeof(sig_val_t));
	if (n != sizeof(conn->nonce)) goto err_out;

	if ((conn->flags & CSTATE_ALL_FD) == CSTATE_ALL_FD)
		have_full_conn(list_state, i);
	return;

err_out:
	if (conn->flags & CSTATE_CNTRL_FD)
		close(conn->control_fd);
	if (conn->flags & CSTATE_DATA_FD)
		close(conn->data_fd);
	conn->flags &= ~(CSTATE_ALLOCATED | CSTATE_CNTRL_FD | CSTATE_DATA_FD);
}


/*
 * This is the main thread for the listener.  This listens for
 * new incoming connection requests and creats the new connection
 * information associated with them.
 */

void
sstub_listen(void *cookie)
{
	listener_state_t *list_state;
	struct timeval  now;
	int		n;
	int		max_fd = 0;
	fd_set		read_fds;

	list_state = (listener_state_t *) cookie;

	FD_ZERO(&read_fds);
	FD_SET(list_state->listen_fd, &read_fds);
	max_fd = list_state->listen_fd + 1;

	now.tv_sec = 1;
	now.tv_usec = 0;

	/*
	 * Sleep on the set of sockets to see if anything
	 * interesting has happened.
	 */
	n = select(max_fd, &read_fds, NULL, NULL, &now);
	if (n == -1) {
		/* XXX log */
		printf("XXX select failed \n");
		exit(1);
	}

	if (n && FD_ISSET(list_state->listen_fd, &read_fds))
		accept_connection(list_state);
}

