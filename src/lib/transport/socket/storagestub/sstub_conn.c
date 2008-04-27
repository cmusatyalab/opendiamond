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

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/xdr.h>
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
#include "rpc_client_content.h"
#include "xdr_shim.h"

/*
 * This sets up a Sun RPC server listening on a random port number.
 */

struct cts_args {
  uint16_t volatile *control_ready;
  uint16_t port;
  cstate_t *cstate;
};

static void *
create_rpc_server(void *arg) {
    SVCXPRT *transp;
    struct addrinfo *ai, hints = {
	.ai_family = AF_UNSPEC,
	.ai_socktype = SOCK_STREAM
    };
    int rpcfd, err;
    char port[NI_MAXSERV];

    struct cts_args *data = (struct cts_args *)arg;

    if(data == NULL) {
      fprintf(stderr, "create_rpc_server: NULL arguments passed\n");
      pthread_exit((void *)-1);
    }

    sprintf(port, "%u", data->port);
    err = getaddrinfo(NULL, port, &hints, &ai);
    if (err) {
	fprintf(stderr, "create_rpc_server: getaddrinfo failed: %s\n",
		gai_strerror(err));
	pthread_exit((void *)-1);
    }

    rpcfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (rpcfd < 0) {
	perror("socket");
	pthread_exit((void *)-1);
    }

    if (bind(rpcfd, ai->ai_addr, ai->ai_addrlen) < 0) {
	perror("bind");
	pthread_exit((void *)-1);
    }

    freeaddrinfo(ai);

    transp = svctcp_create(rpcfd, BUFSIZ, BUFSIZ);
    if (transp == NULL) {
      fprintf (stderr, "%s", "cannot create Sun RPC tcp service.");
      pthread_exit((void *)-1);
    }

    pmap_unset(CLIENTCONTENT_PROG, CLIENTCONTENT_VERS);

    if (!svc_register(transp, CLIENTCONTENT_PROG, CLIENTCONTENT_VERS,
		      clientcontent_prog_2, 0)) {
      fprintf(stderr, "(Sun RPC server) unable to register \"client to "
	      "content\" program (prognum=0x%x, versnum=%d, tcp)\n", 
	      CLIENTCONTENT_PROG, CLIENTCONTENT_VERS);
      pthread_exit((void *)-1);
    }

    *(data->control_ready) = 1;       /* Signal the parent thread that
				       * our Sun RPC server is ready to
				       * accept connections. */

    svc_run();
    fprintf(stderr, "create_rpc_server: svc_run returned!\n");
    pthread_exit((void *)-1);
}


/* 
 * This function spawns a thread which becomes a Sun RPC server, and then
 * makes a local TCP connection to that server.  It returns the socket file
 * descriptor to which the tunnel can write control bits.
 */

static int
setup_rpc(cstate_t *cstate) {
    int error, connfd;
    char port_str[NI_MAXSERV];
    struct addrinfo *info, hints;
    pthread_t rpc_thread;
    struct timeval t;

    volatile uint16_t control_ready = 0;
    struct cts_args *args;

    args = (struct cts_args *)calloc(1, sizeof(struct cts_args));

    /* Choose a random TCP port between 10k and 65k to connect to the 
     * Sun RPC server on.  Use high-order bits for improved randomness. */
    
    gettimeofday(&t, NULL);
    srand(t.tv_sec);
    args->port = 10000 + (int)(55000.0 * (rand()/(RAND_MAX + 1.0)));


    /* Set up a shared variable so the child can signal when it is
     * ready to receive incoming connections.  This variable is volatile
     * to force the compiler to check its value rather than optimize. */

    args->control_ready = &control_ready;


    /* Create a thread which becomes a Sun RPC server. */

    bzero(&rpc_thread, sizeof(pthread_t));
    pthread_create(&rpc_thread, NULL, create_rpc_server, 
		   (void *)args);
    
    while(control_ready == 0) continue;
    
    bzero(&hints,  sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_str, NI_MAXSERV, "%u", args->port);
    
    if((error = getaddrinfo(NULL, port_str, &hints, &info)) < 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
      pthread_exit((void *)-1);
    }

    /* Create new connection to the Sun RPC server. */
    connfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (connfd < 0) {
      perror("socket");
      freeaddrinfo(info);
      pthread_exit((void *)-1);
    }

    if(connect(connfd, info->ai_addr, info->ai_addrlen) < 0) {
      perror("sunrpc connect");
      freeaddrinfo(info);
      pthread_exit((void *)-1);
    }

    freeaddrinfo(info);

    free(args);
    return connfd;
}


/*
 * This is the loop that handles the socket communications for each
 * of the different "connections" to the disk that exist.
 */

void           *
connection_main(listener_state_t * lstate, int conn)
{
	cstate_t       *cstate;
	struct timeval  to;
	int             max_fd;
	int             err;
	fd_set		read_fds;
	fd_set		write_fds;
	fd_set		except_fds;

	cstate = &lstate->conns[conn];


	/*
	 * Create a Sun RPC server thread and then make a TCP
	 * connection to it.
	 */

	cstate->rpc_fd = setup_rpc(cstate);


	/*
	 * Compute the max fd for the set of file
	 * descriptors that we care about.
	 */
	max_fd = cstate->control_fd;
	if (cstate->data_fd > max_fd) {
		max_fd = cstate->data_fd;
	}
	if (cstate->rpc_fd > max_fd) {
		max_fd = cstate->rpc_fd;
	}
	max_fd += 1;

	cstate->lstate = lstate;

	sstub_set_states(lstate, cstate);

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

		FD_SET(cstate->control_fd, &read_fds);
		FD_SET(cstate->data_fd, &read_fds);
		FD_SET(cstate->rpc_fd, &read_fds);

		FD_SET(cstate->control_fd, &except_fds);
		FD_SET(cstate->data_fd, &except_fds);
		FD_SET(cstate->rpc_fd, &except_fds);

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
			 * handle data tunneling between control and ti-rpc
			 */
			if (FD_ISSET(cstate->control_fd, &read_fds))
			  sstub_read_control(lstate, cstate);

			if (FD_ISSET(cstate->rpc_fd, &read_fds))
			  sstub_read_rpc(lstate, cstate);

			/*
			 * handle outgoing data on the data connection
			 */
			if (FD_ISSET(cstate->data_fd, &read_fds)) {
			  sstub_read_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &write_fds)) {
			  sstub_write_data(lstate, cstate);
			}

			/*
			 * handle the exception conditions on the socket
			 */
			if (FD_ISSET(cstate->control_fd, &except_fds)) {
				sstub_except_control(lstate, cstate);
			}
			if (FD_ISSET(cstate->data_fd, &except_fds)) {
				sstub_except_data(lstate, cstate);
			}
			if (FD_ISSET(cstate->rpc_fd, &except_fds)) {
				sstub_except_rpc(lstate, cstate);
			}
		}
	}
}
