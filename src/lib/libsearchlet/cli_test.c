/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include "lib_searchlet.h"


/* XXX debug */
#define	TEST_FILTER_NAME	"/home/larry/coda/src/proj/libsearchlet/test.so"
#define	TEST_FILTER_SPEC	"/home/larry/coda/src/proj/libsearchlet/test_spec"
int
main(int argc , char **argv)
{
	int			err;
	ls_obj_handle_t		cur_obj;
	ls_search_handle_t	shandle;
	groupid_t		gid;


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
		\
	}



	err = ls_set_searchlet(shandle, DEV_ISA_IA32, TEST_FILTER_NAME,
	                       TEST_FILTER_SPEC);
	if (err) {
		printf("Failed to set searchlet on err %d \n", err);
		exit(1);
		\
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

