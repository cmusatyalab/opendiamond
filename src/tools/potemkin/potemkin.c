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



/*
  struct dctl_x {
  unsigned int	dctl_err;  
  int           dctl_opid;      
  unsigned int  dctl_plen;
  unsigned int  dctl_dtype;
  opaque 	dctl_data<>;
  };
*/
int
test_dctl(CLIENT *clnt, u_int  gen) {
	dctl_x arg;
	dctl_return_x *drx;
	char *key = "/path/to/leaf";
	char *value = "here's your data";
	char buf[512];
	
	strcpy(buf, key);
	strcat(buf, value);
	
	fprintf(stderr, "(potemkin) Making \"write leaf\" distributed"
		" control call.. ");
	memset(&arg, 0, sizeof(dctl_x));
	arg.dctl_err = 0;  
	arg.dctl_opid = 1; /* not read by adiskd */
	arg.dctl_plen = strlen(key);
	arg.dctl_dtype = 0;
	arg.dctl_data.dctl_data_val = buf;
	arg.dctl_data.dctl_data_len = strlen(buf) + 1;
        drx = device_write_leaf_x_2(gen, arg, clnt);
	if (drx == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	  return -1;
	}
	fprintf(stderr, "%s\n", diamond_error(&drx->error));
	if(drx->error.service_err != DIAMOND_SUCCESS)
	  return -1;

	fprintf(stderr, "(potemkin) Making \"read leaf\" distributed"
		" control call.. ");
	memset(&arg, 0, sizeof(dctl_x));
	arg.dctl_opid = 1;
	arg.dctl_data.dctl_data_val = key;
	arg.dctl_data.dctl_data_len = strlen(key)+1;
	drx = device_read_leaf_x_2(gen, arg, clnt);
	if (drx == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	  return -1;
	}
	fprintf(stderr, "%s\n", diamond_error(&drx->error));
	if(drx->error.service_err != DIAMOND_SUCCESS)
	  return -1;
	fprintf(stderr, "\treturned %s\n", drx->dctl.dctl_data.dctl_data_val);

#if 0
	fprintf(stderr, "(potemkin) Making \"list nodes\" distributed control"
		" call.. ");
	memset(&arg, 0, sizeof(dctl_x));
	drx = device_list_nodes_x_2(gen, arg, clnt);
	if (drx == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	  return -1;
	}
	fprintf(stderr, "%s\n", diamond_error(&drx->error));
	if(drx->error.service_err != DIAMOND_SUCCESS)
	  return -1;

	fprintf(stderr, "(potemkin) Making \"list leafs (sic)\" distributed"
		" control call.. ");
	memset(&arg, 0, sizeof(dctl_x));
	drx = device_list_leafs_x_2(gen, arg, clnt);
	if (drx == (dctl_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	  return -1;
	}
	fprintf(stderr, "%s\n", diamond_error(&drx->error));
	if(drx->error.service_err != DIAMOND_SUCCESS)
	  return -1;
#endif

	return 0;
}


int
get_chars(CLIENT *clnt, u_int gen) {
	request_chars_return_x *characteristics;

	fprintf(stderr, "(potemkin) Making \"request_characteristics\""
		" call.. ");
	characteristics = request_chars_x_2(gen, clnt);
	if (characteristics == (request_chars_return_x *) NULL) {
	  clnt_perror (clnt, "call failed");
	  return -1;
	}
	fprintf(stderr, "%s\n", diamond_error(&characteristics->error));
	if(characteristics->error.service_err != DIAMOND_SUCCESS)
	  return -1;

	fprintf(stderr, "\tcharacteristic 'dcs_isa' =\t");
	switch(characteristics->chars.dcs_isa) {
	case DEV_ISA_IA32:
	  fprintf(stderr, "IA32\n");
	  break;
	default:
	  fprintf(stderr, "unknown\n");
	  break;
	}

	fprintf(stderr, "\tcharacteristic 'dcs_speed' =\t%u HZ\n",
		characteristics->chars.dcs_speed);
	fprintf(stderr, "\tcharacteristic 'dcs_mem' =\t%llu bytes free\n",
		characteristics->chars.dcs_mem);

	return 0;
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
	  fprintf(stderr, "(potemkin) failed initializing TI-RPC "
		  "client handle.\n");
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
	if(get_chars(clnt, gen) < 0)
	  exit(EXIT_FAILURE);

	test_dctl(clnt, gen);

#if 0

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

	/* XXX - should this be during a search? */
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
