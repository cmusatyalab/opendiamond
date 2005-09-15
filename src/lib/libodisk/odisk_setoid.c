/*
 * 	Diamond (Release 1.0)
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "sig_calc.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "odisk_priv.h"
#include "rtimer.h"


static char const cvsid[] = "$Header$";

uint32_t
get_devid(char * host_addr)
{
	int	err;
	struct in_addr	in;

	err = inet_aton(host_addr, &in);

	return(in.s_addr);
}


int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	int		err;
	uint32_t	devid;
	void *		log_cookie;
	void *		dctl_cookie;
	char *		host_addr;


	host_addr = argv[1];

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, NULL, dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	devid = get_devid(host_addr);
	printf("dev_id %08x \n", devid);

	odisk_write_oids(odisk, devid);

	exit(0);
}
