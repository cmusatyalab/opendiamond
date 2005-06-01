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


#include    <sys/types.h>
#include    <unistd.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <stdint.h>
#include    <assert.h>
#include    "diamond_consts.h"
#include    "diamond_types.h"
#include    "lib_dconfig.h"




int
main(int argc, char **argv)
{
	char *	dir;

	dir = dconf_get_datadir();

	printf("<%s> \n", dir);

	exit(0);
}
