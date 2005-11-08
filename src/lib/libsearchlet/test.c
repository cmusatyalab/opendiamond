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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "lib_tools.h"
#include "lib_searchlet.h"
#include "lib_filter.h"


static char const cvsid[] =
    "$Header$";

int
test(lf_obj_handle_t ohandle, int numarg, char **args)
{
	int             foo = 1;
	int             bar = 2;
	size_t          size;
	int             err;

	printf("num args %d \n", numarg);
	{
		int             i;
		for (i = 0; i < numarg; i++) {
			printf("arg %d <%s> \n", i, args[i]);
		}

	}

	lf_write_attr(ohandle, "test", sizeof(foo), (char *) &foo);

	size = sizeof(bar);
	err = lf_read_attr(ohandle, "test", &size, (char *) &bar);
	if (err != 0) {
		/*
		 * XXX error handler 
		 */
		exit(1);
	}
	if (bar != foo) {
		printf("read/write attr not match: %d %d \n", foo, bar);
	}

	foo = 45;
	lf_write_attr(ohandle, "test", sizeof(foo), (char *) &foo);
	size = sizeof(bar);
	err = lf_read_attr(ohandle, "test", &size, (char *) &bar);
	if (err != 0) {
		/*
		 * XXX error handler 
		 */
		exit(1);
	}
	if (bar != foo) {
		printf("read/write attr not match: %d %d \n", foo, bar);
	}

	return (0);
}
