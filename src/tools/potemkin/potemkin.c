/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * This file was adapted from lib/transport/socket/hoststub/htest.c
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <tirpc/rpc/types.h>
#include <netconfig.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_log.h"
#include "rpc_client_content.h"

#define CONTROL_PORT 5872
#define DATA_PORT 5873

#if 0
void
test_dctl(u_int  gen) {
	diamond_rc_t *rc;
	dctl_x device_write_leaf_x_2_arg2;
	dctl_x device_read_leaf_x_2_arg2;
	
	rc = device_write_leaf_x_2(gen, device_write_leaf_x_2_arg2, clnt);
	if (rc == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	
	rc = device_read_leaf_x_2(gen, device_read_leaf_x_2_arg2, clnt);
	if (rc == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	rc = device_list_nodes_x_2(gen, device_list_nodes_x_2_arg2, clnt);
	if (rc == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	rc = device_list_leafs_x_2(gen, device_list_leafs_x_2_arg2, clnt);
	if (rc == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
}
#endif


int
create_tcp_connection(char *hostname, int port)
{
	int sockfd, error;
	struct addrinfo hints, *info;
	char port_str[6];
	
	if((hostname == NULL) || ((port < 0) || (port > 65535)))
	  return -1;

	/* create TCP socket */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror("socket");
	  return -1;
	}
	
	/* get server information */
	bzero(&hints,  sizeof(struct addrinfo));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(port_str, NI_MAXSERV, "%u", port);
	
	if((error =
	    getaddrinfo(hostname, port_str, &hints, &info)) < 0) {
	  fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
	  return -1;
	}
	
	/* make connection */
	if(connect(sockfd, info->ai_addr, sizeof(struct sockaddr_in)) < 0) {
	  perror("connect");
	  return -1;
	}
	
	freeaddrinfo(info);
	
	return sockfd;
}


CLIENT *
tirpc_init(int connfd, unsigned int *cookie) {
	struct sockaddr control_name;
	unsigned int control_name_len = sizeof(struct sockaddr);
	struct netconfig *nconf;
	struct netbuf *tbind;
	CLIENT *clnt;

	nconf = getnetconfigent("tcp");
	if(nconf == NULL) {
	  perror("getnetconfigent");
	  exit(EXIT_FAILURE);
	}
	
	/* Transform sockaddr_in to netbuf */
	tbind = (struct netbuf *) malloc(sizeof(struct netbuf));
	if(tbind == NULL) {
	  perror("malloc");
	  exit(EXIT_FAILURE);
	}
	
	tbind->buf = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	if(tbind->buf == NULL) {
	  perror("malloc");
	  exit(EXIT_FAILURE);
	}
	if(getsockname(connfd, &control_name, &control_name_len) < 0) {
	  perror("getsockname");
	  exit(EXIT_FAILURE);
	}
	memcpy(tbind->buf, &control_name, sizeof(struct sockaddr));
	tbind->maxlen = tbind->len = sizeof(struct sockaddr);
	
	if ((clnt = clnt_tli_create(connfd,
					      nconf,
					      tbind, 
					      OPENDIAMOND_PROG, 
					      OPENDIAMOND_VERS, 
					      BUFSIZ, BUFSIZ)) == NULL) {
	  clnt_pcreateerror("clnt_tli_create");
	  fprintf(stderr, "client: error creating TI-RPC tcp client\n");
	  exit(EXIT_FAILURE);
	}
	
	printf("TI-RPC client successfully created and connected!\n");

	return clnt;
}


int 
data_connect(char *hostname, unsigned int cookie) {
	int connfd, size;

	connfd = create_tcp_connection(hostname, DATA_PORT);

	/* Write the 32-bit cookie back to the server. */
	size = write(connfd, (char *) &cookie, sizeof(cookie));
	if (size == -1) {
	  close(connfd);
	  return -1;
	}

	return connfd;
}


int
control_connect(char *hostname, unsigned int *cookie) {
	int connfd, size;
	
	connfd = create_tcp_connection(hostname, CONTROL_PORT);
	
	/* Read 32-bit cookie from server. */
	size = read(connfd, (char *)cookie, sizeof(*cookie));
	if (size == -1) {
	  printf("Failed reading control connection cookie.\n");
	  close(connfd);
	  return -1;
	}
	
	return connfd;
}


/* For the moment, potemkin only makes raw TI-RPC calls to adiskd
 * rather than calling into the client-side (hoststub) library.
 * This will change when the client-side library is adapted to
 * use TI-RPC calls since it allows for a more semantically meaningful
 * debugging of the server. */

int
main(int argc, char **argv)
{
	char *hostname;
	CLIENT *clnt;
	int controlfd, datafd;
	diamond_rc_t *rc;
	u_int gen, mode, state;
	unsigned int cookie;
	stop_x stats;

	if(argc != 2) {
	  printf("usage: %s [hostname]\n", argv[0]);
	  exit(EXIT_SUCCESS);
	}
	
	hostname = argv[1];
	if((controlfd = control_connect(hostname, &cookie)) < 0) {
	  printf("Failed initializing control connection.\n");
	  exit(EXIT_FAILURE);
	}
	if((datafd = data_connect(hostname, cookie)) < 0) {
	  printf("Failed initializing control connection.\n");
	  exit(EXIT_FAILURE);
	}
	if((clnt = tirpc_init(controlfd, &cookie)) == NULL) {
	  printf("Failed mutating control connection into "
		 "TI-RPC connection.\n");
	  exit(EXIT_FAILURE);
	}
	

#if 0
	gen = 100;

	rc = request_chars_x_2(gen, clnt);
	if (rc == (request_chars_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	test_dctl(gen);

	rc = device_set_obj_x_2(gen, device_set_obj_x_2_arg2, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	rc = device_send_obj_x_2(gen, device_send_obj_x_2_arg2, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	rc = device_set_spec_x_2(gen, device_set_spec_x_2_arg2, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	rc = device_set_blob_x_2(gen, device_set_blob_x_2_arg2, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	rc = device_clear_gids_x_2(gen, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	rc = device_new_gid_x_2(gen, device_new_gid_x_2_arg2, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	rc = device_set_exec_mode_x_2(gen, mode, clnt);
	if (result_12 == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	rc = device_start_x_2(gen++, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	rc = device_set_user_state_x_2(gen, state, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	stats.host_objs_received = 0;
	stats.host_objs_queued = 0;
	stats.host_objs_read = 0;
	stats.app_objs_queued = 0;
	stats.app_objs_presented = 0;

	rc = device_stop_x_2(gen, stats, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
	
	/* XXX - should this be during a search? */
	rc = request_stats_x_2(gen, clnt);
	if (rc == (request_stats_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	}

	rc = device_terminate_x_2(gen, clnt);
	if (rc == (diamond_rc_t *) NULL) {
	  clnt_perror (clnt, "call failed");
	}
#endif

	if(clnt != NULL)
	  clnt_destroy (clnt);

	close(controlfd);
	close(datafd);
	
	return 0;
}
