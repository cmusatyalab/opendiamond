/*
 *      Diamond
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
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "odisk_priv.h"


static char const cvsid[] =
    "$Header$";


int
main(int argc, char **argv)
{
	odisk_state_t  *odisk;
	int             err;

	err = odisk_init(&odisk, NULL, NULL, NULL);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	err = odisk_clear_indexes(odisk);
	if (err) {
		errno = err;
		perror("Failed to clear indexes \n");
		exit(1);
	}

	err = odisk_build_indexes(odisk);
	if (err) {
		errno = err;
		perror("Failed to build indexes \n");
		exit(1);
	}
	exit(0);
}
