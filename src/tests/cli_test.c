/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include "lib_tools.h"
#include "lib_searchlet.h"

/*
 * XXX debug 
 */
#define	TEST_FILTER_NAME	"/Users/huston/diamond/diamond/src/lib/libsearchlet/test.so"
#define	TEST_FILTER_SPEC	"/home/larry/coda/src/proj/libsearchlet/test_spec"
int
main(void)
{
	int             err;
	ls_obj_handle_t cur_obj;
	ls_search_handle_t shandle;
	groupid_t       gid;


	shandle = ls_init_search();
	if (shandle == NULL) {
		printf("failed to initialize:  !! \n");
		exit(1);
	}

	/*
	 * set the set of items we want to search.
	 * XXX figure out the correct set of functions.
	 */
	gid = 1;
	err = ls_set_searchlist(shandle, 1, &gid);
	if (err) {
		printf("Failed to set searchlet on err %d \n", err);
		exit(1);

	}



	err = ls_set_searchlet(shandle, DEV_ISA_IA32, TEST_FILTER_NAME,
			       TEST_FILTER_SPEC);
	if (err) {
		printf("Failed to set searchlet on err %d \n", err);
		exit(1);

	}

	/*
	 * Go ahead and start the search.
	 */
	err = ls_start_search(shandle);
	if (err) {
		printf("Failed to start search on err %d \n", err);
		exit(1);
	}

	/*
	 * Get all the objects that match.
	 */
	while (1) {
		err = ls_next_object(shandle, &cur_obj, 0);
		if (err == ENOENT) {
			printf("No more objects \n");
			break;

		} else if (err != 0) {
			printf("get_next_obj: failed on %d \n", err);
			exit(1);
		}
		printf("obj!\n");
		err = ls_release_object(shandle, cur_obj);
		if (err) {
			printf("failed to release object \n");

		}

	}




	return (0);
}
