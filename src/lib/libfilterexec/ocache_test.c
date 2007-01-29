/*
 *      OpenDiamond 2.0
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
#include "lib_tools.h"
#include "lib_od.h"
#include "lib_ocache.h"
#include "ocache_priv.h"


static char const cvsid[] =
    "$Header$";

int
main(int argc, char **argv)
{
	ocache_state_t *ocache;
	void           *log_cookie;
	void           *dctl_cookie;
	int             err;
	char            fsig[16] = { "ANIMOWLOEMOKLWML" };
	char            iattr_sig[16] = { "VNIMOWLOEMOKLWML" };
	char            oattr_sig[16] = { "BNIMOWLOEMOKLWML" };
	unsigned char   signature[16];

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = ocache_init(NULL, dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init ocache");
		exit(1);
	}

	ocache_start();

	printf("cache test done \n");
	exit(0);
}
