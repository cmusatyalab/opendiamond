/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2007 Carnegie Mellon University
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
#include "lib_dctl.h"
#include "dctl_common.h"
#include "lib_auth.h"
#include "lib_sstub.h"
#include "sstub_impl.h"


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


	dctl_register_leaf(DEV_NETWORK_PATH, "obj_sent", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &cstate->stats_objs_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "obj_tot_bytes_sent",
			   DCTL_DT_UINT64, dctl_read_uint64, NULL,
			   &cstate->stats_objs_total_bytes_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "obj_data_bytes_sent",
			   DCTL_DT_UINT64, dctl_read_uint64, NULL,
			   &cstate->stats_objs_data_bytes_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "obj_attr_bytes_sent",
			   DCTL_DT_UINT64, dctl_read_uint64, NULL,
			   &cstate->stats_objs_attr_bytes_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "obj_hdr_bytes_sent",
			   DCTL_DT_UINT64, dctl_read_uint64, NULL,
			   &cstate->stats_objs_hdr_bytes_tx);


	dctl_register_leaf(DEV_NETWORK_PATH, "control_sent", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &cstate->stats_control_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "control_bytes_sent",
			   DCTL_DT_UINT64, dctl_read_uint64, NULL,
			   &cstate->stats_control_bytes_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "control_recv", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &cstate->stats_control_rx);
	dctl_register_leaf(DEV_NETWORK_PATH, "control_bytes_recv",
			   DCTL_DT_UINT64, dctl_read_uint64, NULL,
			   &cstate->stats_control_bytes_rx);
	dctl_register_leaf(DEV_NETWORK_PATH, "log_sent", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &cstate->stats_log_tx);
	dctl_register_leaf(DEV_NETWORK_PATH, "log_bytes_sent", DCTL_DT_UINT64,
			   dctl_read_uint64, NULL,
			   &cstate->stats_log_bytes_tx);

	dctl_register_leaf(DEV_NETWORK_PATH, "attr_policy", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &cstate->attr_policy);
	dctl_register_leaf(DEV_NETWORK_PATH, "attr_ratio", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &cstate->attr_ratio);
	dctl_register_leaf(DEV_NETWORK_PATH, "attr_ratio", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &cstate->attr_ratio);
	dctl_register_leaf(DEV_NETWORK_PATH, "cc_credits", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &cstate->cc_credits);

}

/*
 * This is called if the connection has been shutdown the remote
 * side or some error connection.  We notify the caller of the library
 * and clean up the state.
 */

void
shutdown_connection(listener_state_t * lstate, cstate_t * cstate)
{

	/*
	 * set the flag to indicate we are shutting down 
	 */
	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags |= CSTATE_SHUTTING_DOWN;
	pthread_mutex_unlock(&cstate->cmutex);


	/*
	 * Notify the "application" through the callback.
	 */
	(*lstate->close_conn_cb) (cstate->app_cookie);


	/*
	 * if there is a control socket, close it 
	 */
	if (cstate->flags & CSTATE_CNTRL_FD) {
		close(cstate->control_fd);
		cstate->flags &= ~CSTATE_CNTRL_FD;
	}

	/*
	 * if there is a data socket, close it 
	 */
	if (cstate->flags & CSTATE_DATA_FD) {
		close(cstate->data_fd);
		cstate->flags &= ~CSTATE_DATA_FD;
	}

	/*
	 * clear the established flag 
	 */
	cstate->flags &= ~CSTATE_ESTABLISHED;
	cstate->flags &= ~CSTATE_ALLOCATED;

	/*
	 * XXX do we need this ?? 
	 */
	cstate->flags &= ~CSTATE_SHUTTING_DOWN;

	/*
	 * XXX we need to clean up other state such as queued data .... 
	 */
}


/*
 * Create and establish a socket with the other
 * side.
 */
int
sstub_new_sock(int *fd, int port, int bind_only_locally)
{
	int             new_fd;
	struct protoent *pent;
	struct sockaddr_in sa;
	int             err;
	int             yes = 1;

	pent = getprotobyname("tcp");
	if (pent == NULL) {
		/*
		 * XXX log error 
		 */
		return (ENOENT);
	}

	new_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);
	/*
	 * XXX err ?? 
	 */


	/*
	 * set the socket options to avoid the re-use message if
	 * we have a fast restart.
	 */

	err = setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (err == -1) {
		/*
		 * XXX log 
		 */
		perror("setsockopt");
		return (ENOENT);
	}


	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short) port);

	if (bind_only_locally) {
		sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	} else {
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	err = bind(new_fd, (struct sockaddr *) &sa, sizeof(sa));
	if (err) {
		/*
		 * XXX log 
		 */
		perror("bind failed ");
		return (ENOENT);
	}

	err = listen(new_fd, SOMAXCONN);
	if (err) {
		/*
		 * XXX log 
		 */
		printf("bind failed \n");
		return (ENOENT);
	}

	*fd = new_fd;
	return (0);
}


/*
 * We have all the sockets endpoints open for this new connetion.
 * Now we span a thread for managing this connection as well
 * using the callbacks to inform the caller that there is a new
 * connection to be serviced.
 */
static void
have_full_conn(listener_state_t * list_state, int conn)
{

	int             err;
	void           *new_cookie;
	cstate_t       *cstate;
	int				parent;
	unsigned int 	addr_len;

	/* set up connection info */
	cstate = &list_state->conns[conn];
	cstate->cinfo.conn_idx = conn;
	timerclear(&cstate->cinfo.connect_time);
	gettimeofday(&cstate->cinfo.connect_time, NULL);
	addr_len = sizeof(cstate->cinfo.clientaddr);
	err = getpeername(cstate->control_fd, 
			  (struct sockaddr *)&cstate->cinfo.clientaddr,
			  &addr_len);
	if (err) {
 	    log_message(LOGT_NET, LOGL_ERR, 
					"Error %d while calling getpeername", err);
	}

	err = ring_2init(&cstate->complete_obj_ring, OBJ_RING_SIZE);
	if (err) {
		/*
		 * XXX 
		 */
		printf("failed to init obj ring \n");
		return;
	}

	err = ring_2init(&cstate->partial_obj_ring, OBJ_RING_SIZE);
	if (err) {
		/*
		 * XXX 
		 */
		printf("failed to init obj ring \n");
		return;
	}

	err = ring_init(&cstate->control_tx_ring, CONTROL_RING_SIZE);
	if (err) {
		/*
		 * XXX 
		 */
		printf("failed to init control ring \n");
		return;
	}

	parent = (*list_state->new_conn_cb) ((void *) cstate, &new_cookie);
	if (parent) {
		shutdown_connection(list_state, cstate);
	} else {
		/*
		 * we registered correctly, save the cookie;
		 */
		list_state->conns[conn].app_cookie = new_cookie;

		/*
		 * Register the statistics with dctl.  This needs to be
		 * done after the new_conn_cb()  !!!.
		 */
		register_stats(cstate);

		cstate->attr_policy = DEFAULT_NW_ATTR_POLICY;
		cstate->attr_threshold = RAND_MAX;
		cstate->attr_ratio = DEFAULT_NW_ATTR_RATIO;

		/*
		 * the main thread for this process is used
		 * for servicing the connections.
		 */
		connection_main(list_state, conn);
		return;
	}
}


/*
 * This accepts an incomming connection request to the control
 * port.  We accept the connection and assign it to a new connection
 * state.
 */

static void
accept_control_conn(listener_state_t * list_state)
{
	struct sockaddr_in ca;
	socklen_t       csize;
	int             new_sock;
	int             i;
	int				len;
	uint32_t        data;
	ssize_t         wsize;
	char 			buf[BUFSIZ];

	csize = sizeof(ca);
	new_sock = accept(list_state->control_fd, (struct sockaddr *)
			  &ca, &csize);

	if (new_sock < 0) {
		/*
		 * XXX log 
		 */
		printf("XXX accept failed \n");
	}
	
	/* authenticate connection */
	if (list_state->flags & LSTATE_AUTH_REQUIRED) {
		/* send a negative cookie to alert the host */
		data = -1;
		wsize = write(new_sock, (char *) &data, sizeof(data));
		if (wsize < 0) {
			/*
			 * XXX log 
		 	*/
			printf("XXX Failed write on cntrl connection \n");
			close(new_sock);
			return;
		}
		
		list_state->ca_handle = auth_conn_server(new_sock);
		if (list_state->ca_handle == NULL) {
			close(new_sock);
			printf("Authentication failed (control)\n");
			return;
		}
	}
		
	/*
	 * Now we allocate a per connection state information and
	 * store the socket associated with this.
	 */
	for (i = 0; i < MAX_CONNS; i++) {
		if (!(list_state->conns[i].flags & CSTATE_ALLOCATED)) {
			break;
		}
	}
	if (i == MAX_CONNS) {
		/*
		 * XXX log 
		 */
		printf("XXX accept control no state \n");
		close(new_sock);
		return;
	}

	list_state->conns[i].flags |= CSTATE_ALLOCATED;
	list_state->conns[i].flags |= CSTATE_CNTRL_FD;
	list_state->conns[i].control_fd = new_sock;
	list_state->conns[i].log_tx_buf = NULL;
	list_state->conns[i].log_tx_state = LOG_TX_NO_PENDING;
	list_state->conns[i].control_rx_state = CONTROL_RX_NO_PENDING;
	list_state->conns[i].control_tx_state = CONTROL_TX_NO_PENDING;
	list_state->conns[i].data_tx_state = DATA_TX_NO_PENDING;

	/*
	 * XXX return value ??
	 */
	pthread_mutex_init(&list_state->conns[i].cmutex, NULL);

	data = (uint32_t) i;
	
	if (list_state->flags & LSTATE_AUTH_REQUIRED) {
		len = auth_msg_encrypt(list_state->ca_handle, (char *) &data, sizeof(data),
							buf, BUFSIZ);
		if (len < 0) {
			printf("Error while encrypting message\n");
			close(new_sock);
			list_state->conns[i].flags &= ~CSTATE_ALLOCATED;
			return;
		}
	
		wsize = write(new_sock, &buf[0], len);
		if (wsize < 0) {
			printf("Failed write on cntrl connection\n");
			close(new_sock);
			list_state->conns[i].flags &= ~CSTATE_ALLOCATED;
			return;
		}
	
    } else {
		wsize = write(new_sock, (char *) &data, sizeof(data));
		if (wsize < 0) {
		/*
		 * XXX log 
		 */
			printf("XXX Failed write on cntrl connection \n");
			close(new_sock);
			list_state->conns[i].flags &= ~CSTATE_ALLOCATED;
			return;
		}
	}
	
	return;
}

/*
 * This accepts an incomming connection request to the data
 * port.  We accept the connection and get some data out of it.  This
 * should tell us what connection it belongs to (if the caller did
 * the correct handshake.  
 */

static void
accept_data_conn(listener_state_t * list_state)
{
	struct sockaddr_in ca;
	socklen_t	csize;
	int             new_sock;
	uint32_t        data;
	ssize_t          size, dsize;
	char 			buf[BUFSIZ];

	csize = sizeof(ca);
	new_sock = accept(list_state->data_fd, (struct sockaddr *)
			  &ca, &csize);

	if (new_sock < 0) {
		/*
		 * XXX log 
		 */
		printf("XXX accept failed \n");
	}
	
	/* authenticate connection */
	if (list_state->flags & LSTATE_AUTH_REQUIRED) {
		list_state->da_handle = auth_conn_server(new_sock);
		if (list_state->da_handle == NULL) {
			close(new_sock);
			printf("Authentication failed (data)\n");
			return;
		}
		
		/* read the encrypted cookie */
		size = read(new_sock, &buf[0], BUFSIZ);
		if (size == -1) {
			printf("failed to read from socket");
			close(new_sock);
			return;
		}
	
		printf("Read encrypted message of %d bytes\n", size);

		/* decrypt the message */	
		dsize = auth_msg_decrypt(list_state->da_handle, buf, size, 
							(char *) &data, sizeof(data));		
	} else {
		dsize = read(new_sock, (char *) &data, sizeof(data));
	}
	
	if (dsize < 0) {
		/*
		 * XXX 
		 */
		printf("failed read cookie \n");
		close(new_sock);
		return;
	}

	if (data >= MAX_CONNS) {
		/*
		 * XXX 
		 */
		fprintf(stderr, "data conn cookie out of range <%d> \n",
			data);
		close(new_sock);
		return;
	}


	if (!(list_state->conns[data].flags & CSTATE_ALLOCATED)) {
		/*
		 * XXX 
		 */
		fprintf(stderr, "connection not on valid cookie <%d>\n",
			data);
		close(new_sock);
		return;
	}


	list_state->conns[data].flags |= CSTATE_DATA_FD;
	list_state->conns[data].data_fd = new_sock;

	socket_non_block(new_sock);

	if ((list_state->conns[data].flags & CSTATE_ALL_FD) == CSTATE_ALL_FD) {
		have_full_conn(list_state, (int) data);

	}

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
	int             err;
	int             max_fd = 0;

	list_state = (listener_state_t *) cookie;

	max_fd = list_state->control_fd;
	if (list_state->data_fd > max_fd) {
		max_fd = list_state->data_fd;
	}
	max_fd += 1;

	FD_ZERO(&list_state->read_fds);
	FD_ZERO(&list_state->write_fds);
	FD_ZERO(&list_state->except_fds);

	FD_SET(list_state->control_fd, &list_state->read_fds);
	FD_SET(list_state->data_fd, &list_state->read_fds);

	now.tv_sec = 1;
	now.tv_usec = 0;

	/*
	 * Sleep on the set of sockets to see if anything
	 * interesting has happened.
	 */
	err = select(max_fd, &list_state->read_fds,
		     &list_state->write_fds,
		     &list_state->except_fds, &now);
	if (err == -1) {
		/*
		 * XXX log 
		 */
		printf("XXX select failed \n");
		exit(1);
	}

	/*
	 * If err > 0 then there are some objects
	 * that have data.
	 */
	if (err > 0) {
		if (FD_ISSET(list_state->control_fd,
			     &list_state->read_fds)) {
			accept_control_conn(list_state);
		}

		if (FD_ISSET(list_state->data_fd,
			     &list_state->read_fds)) {
			accept_data_conn(list_state);
		}
	}

	return;	
}
