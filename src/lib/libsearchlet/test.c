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
#include <stdlib.h>
#include <stdint.h>
#include "lib_searchlet.h"
#include "lib_filter.h"

int
test(lf_obj_handle_t ohandle, int numarg, char **args)
{
	int		foo = 1;
	int		bar = 2;
	off_t		size;
	off_t		err;
	char *		data;
	lf_fhandle_t	fhandle;

	/* try to map data into memory */
	/* XXXX 4096 */
#ifdef	XXX

	size = 4096;
	err = lf_read_object(fhandle, ohandle, 0, &size, &data);
	if (err) {
		printf("failed to map data \n");
		exit (1);
	}

	err = lf_free_buffer(data);
	if (err) {
		printf("failed to unmap data \n");
		exit (1);
	}
#endif
	printf("num args %d \n", numarg);
	{
		int i;
		for (i=0; i<numarg; i++) {
			printf("arg %d <%s> \n", i, args[i]);
		}

	}

	lf_write_attr(fhandle, ohandle, "test", sizeof(foo), (char *)&foo);

	size = sizeof(bar);
	err = lf_read_attr(fhandle, ohandle,  "test", &size, (char *)&bar);
	if (err != 0) {
		/* XXX error handler  */
		exit(1);
	}
	if (bar != foo) {
		printf("read/write attr not match: %d %d \n", foo, bar);
	}

	foo = 45;
	lf_write_attr(fhandle, ohandle, "test", sizeof(foo), (char *)&foo);
	size = sizeof(bar);
	err = lf_read_attr(fhandle, ohandle,  "test", &size, (char *)&bar);
	if (err != 0) {
		/* XXX error handler  */
		exit(1);
	}
	if (bar != foo) {
		printf("read/write attr not match: %d %d \n", foo, bar);
	}

	return(0);
}



