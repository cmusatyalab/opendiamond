/*
 *      Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
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
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_log.h"


static char const cvsid[] =
    "$Header$";

int
handle_new_obj(void *hcookie, obj_data_t * odata, int vno)
{

	printf("new_obj!! - vno  %d \n", vno);
	return (0);
}

void
handle_log_data(void *hcookie, char *data, int len, int dev)
{
	printf("have log data \n");

}

int
main(int argc, char **argv)
{
	int             err;
	void           *cookie;
	void           *log_cookie;
	void           *dctl_cookie;
	hstub_cb_args_t cb_args;
	struct in_addr  addr;


	cb_args.log_data_cb = handle_log_data;

	err = inet_aton("127.0.0.1", &addr);

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	/*
	 * XXX 
	 */
	cookie = device_init(100, addr.s_addr, 0, &cb_args, dctl_cookie,
			     log_cookie);


	err = device_start(cookie, 101);
	if (err) {
		printf("failed to start device \n");
	}

	err = device_set_searchlet(cookie, 102, "/tmp/x", "/tmp/y");

	printf("XXX sending stop \n");
	err = device_stop(cookie, 102);
	if (err) {
		printf("failed to stop device \n");
	}

	printf("sleeping \n");
	sleep(10);
	printf("done sleep \n");

	exit(0);
}
