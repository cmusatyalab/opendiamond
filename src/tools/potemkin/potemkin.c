/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
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
#include "hstub_impl.h"
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


char *
diamond_error(diamond_rc_t *rc) {
	static char buf[128];

	if(rc == NULL)
	  return NULL;

	switch(rc->service_err) {
	case DIAMOND_SUCCESS:
	  sprintf(buf, "RPC call succeeded.");
	  break;
	case DIAMOND_FAILURE:
	  sprintf(buf,"RPC call failed generically.");
	  break;  
	case DIAMOND_NOMEM:
	  sprintf(buf, "RPC call failed from an out-of-memory error.");
	  break;
	case DIAMOND_FAILEDSYSCALL:
	  sprintf(buf, "RPC call failed from a failed system call: %s", 
		  strerror(rc->opcode_err));
	  break;
	case DIAMOND_OPERR:
	  sprintf(buf, "RPC call failed from an opcode-specific error.");
	  break;
	}

	return buf;
}


/* For the moment, potemkin only makes raw TI-RPC calls to adiskd
 * rather than calling into the client-side (hoststub) library.
 * This will change when the client-side library is adapted to
 * use TI-RPC calls since it allows for a more semantically meaningful
 * debugging of the server. */

int
main(int argc, char **argv)
{
	diamond_rc_t *rc;
	char *hostname;
	u_int gen, mode, state;
	uint32_t devid;
	struct in_addr **addr_list, in;
	conn_info_t cinfo;
	CLIENT *clnt;
	struct hostent *hent;

	stop_x stats;
	request_chars_return_x *characteristics;

	if(argc != 2) {
	  fprintf(stderr, "usage: %s [hostname]\n", argv[0]);
	  exit(EXIT_SUCCESS);
	}
	
	hostname = argv[1];

	if((hent = gethostbyname(hostname)) == 0) {
	  fprintf(stderr, "couldn't resolve hostname %s\n", hostname);
	  exit(EXIT_FAILURE);
	}
	addr_list = (struct in_addr **) (hent->h_addr_list);
	in = *(addr_list[0]);
	devid = in.s_addr;

	
	/*
	 * Test hoststub's connection pairing mechanisms.
	 */
	memset(&cinfo, 0, sizeof(conn_info_t));
	if(hstub_establish_connection(&cinfo, devid) != 0) {
	  fprintf(stderr, "(potemkin) failed establishing connections to server.\n");
	  exit(EXIT_FAILURE);
	}
	clnt = cinfo.tirpc_client;
	if(clnt == NULL) {
	  fprintf(stderr, "(potemkin) failed initializing TI-RPC client handle.\n");
	  exit(EXIT_FAILURE);
	}
	fprintf(stderr, "(potemkin) Connections established.\n");


	/*
	 * Not sure how gen is used just yet besides it increasing.
	 */
	gen = 100;


	/*
	 * Start RPC test by asking for server's characteristics.
	 */
	fprintf(stderr, "(potemkin) Making \"request_characteristics\" call.. ");
	characteristics = request_chars_x_2(gen, clnt);
	if (characteristics == (request_chars_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	  return -1;
	}
	printf("%s\n", diamond_error(&characteristics->error));

#if 0
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
	
	return 0;
}
