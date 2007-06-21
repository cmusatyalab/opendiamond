/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
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
#include <tirpc/rpc/types.h>
#include <rpc/xdr.h>
#include <netconfig.h>
#include <netinet/in.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_auth.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "sstub_impl.h"
#include "rpc_client_content.h"
#include "xdr_shim.h"

static char const cvsid[] =
    "$Header$";

int control_ready, local_port;
cstate_t *tirpc_cstate;
listener_state_t *tirpc_lstate;


void handle_requests(int fd) {
  fd_set read_fds;

  for( ; ; ) {
    int err;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    err = select(fd+1, &read_fds, NULL, NULL, NULL);
    switch(err) {

    case -1:
      perror("select");
      return;

    case 1:
      svc_getreq_common(fd);

    case 0:
    default:
      continue;
    }
  }
}


/*
 * This sets up a TI-RPC server listening on a random port number.
 */

void *
create_tirpc_server(void *arg) {
    struct netconfig *nconf;
    SVCXPRT *transp;
    struct t_bind tbind;
    struct sockaddr_in servaddr;
    struct timeval t;
    int rpcfd, local_port;
    cstate_t       *cstate;

    cstate = (cstate_t *) arg;

    /* Choose a random TCP port between 10k and 65k to connect to the 
     * TI-RPC server on.  Use high-order bits for improved randomness. */
    
    gettimeofday(&t, NULL);
    srand(t.tv_sec);
    local_port = 10000 + (int)(55000.0 * (rand()/(RAND_MAX + 1.0)));


    nconf = getnetconfigent("tcp");
    if(nconf == NULL) {
      perror("getnetconfigent");
      exit(EXIT_FAILURE);
    }

    if((rpcfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("socket");
      exit(EXIT_FAILURE);
    }
  
    bzero(&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* only local conns */
    servaddr.sin_port = htons(local_port);

    tbind.qlen=8;
    tbind.addr.maxlen = tbind.addr.len = sizeof(struct sockaddr_in);
    tbind.addr.buf = &servaddr;
    
    transp = svc_tli_create(rpcfd, nconf, &tbind, BUFSIZ, BUFSIZ);
    if (transp == NULL) {
      fprintf (stderr, "%s", "cannot create tcp service.");
      exit(1);
    }
    
    svc_unreg(OPENDIAMOND_PROG, OPENDIAMOND_VERS);
    
    if (!svc_reg(transp, OPENDIAMOND_PROG, OPENDIAMOND_VERS, 
		 opendiamond_prog_2, NULL)) {
      fprintf(stderr, "%s", "unable to register (OPENDIAMOND_PROG, "
	      "OPENDIAMOND_VERS, tcp).");
      exit(1);
    }

    
    cstate->tirpc_port = local_port; /* Signal the parent thread that
				      * our TI-RPC server is ready to
				      * accept connections. */

    /* Bugs were found in svc_run() in TI-RPC v0.1.7.  A bug report
     * has been filed.  We are switching to our own loop around a
     * lower-level call until it is fixed. */
    //svc_run();

    handle_requests(rpcfd);

    printf("XXX handle_requests returned!\n");
    exit(EXIT_FAILURE);
}


int
create_tirpc_conn(cstate_t *cstate) {
    int error, connfd;
    char port_str[6];
    struct addrinfo *info, hints;
    pthread_t tirpc_thread;
    
    /* Create a thread which becomes a TI-RPC server. */
    control_ready = 0;
    bzero(&tirpc_thread, sizeof(pthread_t));
    pthread_create(&tirpc_thread, PATTR_DEFAULT, create_tirpc_server, 
		   (void *)cstate);
    
    
    /* Wait for the control thread to finish initialization. */
    while(control_ready == 0)
      continue;
    
    
    /* Create new connection to the TI-RPC server. */
    if((connfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("socket");
      exit(EXIT_FAILURE);
    }
    
    bzero(&hints,  sizeof(struct addrinfo));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_str, NI_MAXSERV, "%u", local_port);
    
    if((error = getaddrinfo("localhost", port_str, &hints, &info)) < 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
      exit(EXIT_FAILURE);
    }
    
    if(connect(connfd, info->ai_addr, sizeof(struct sockaddr_in)) < 0) {
      perror("connect");
      exit(EXIT_FAILURE);
    }
    
    return connfd;
}


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
	 * Create a TI-RPC server thread and then make a TCP
	 * connection to it.
	 */
	cstate->tirpc_fd = (int) create_tirpc_server(cstate);

	/*
	 * Compute the max fd for the set of file
	 * descriptors that we care about.
	 */
	max_fd = cstate->control_fd;
	if (cstate->data_fd > max_fd) {
		max_fd = cstate->data_fd;
	}
	if (cstate->tirpc_fd > max_fd) {
		max_fd = cstate->tirpc_fd;
	}
	max_fd += 1;

	cstate->lstate = lstate;

	tirpc_cstate = cstate;
	tirpc_lstate = lstate;

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
		FD_SET(cstate->tirpc_fd, &cstate->read_fds);

		FD_SET(cstate->control_fd, &cstate->except_fds);
		FD_SET(cstate->data_fd, &cstate->except_fds);
		FD_SET(cstate->tirpc_fd, &cstate->except_fds);


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
		pthread_mutex_unlock(&cstate->cmutex);

		to.tv_sec = 1;
		to.tv_usec = 0;

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
			if (FD_ISSET(cstate->tirpc_fd, &cstate->read_fds)) {
				sstub_read_tirpc(lstate, cstate);
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
			if (FD_ISSET(cstate->tirpc_fd, &cstate->except_fds)) {
				sstub_except_tirpc(lstate, cstate);
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
		}
	}
}
