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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include "lib_dctl.h"


/*
 * helper function that returns the value of the
 * uint32_t stored at the cookie location.
 */
int
dctl_read_uint32(void *cookie, int *len, char *data)
{

	assert(cookie != NULL);
	assert(data != NULL);

	if (*len < sizeof(uint32_t)) {
		*len = sizeof(uint32_t);
		return(ENOMEM);
	}


	*len = sizeof(uint32_t);
	*(uint32_t *)data = *(uint32_t *)cookie;

	return(0);	
}


/*
 * helper function that write the value passed in the data to the
 * uint32_t that the cookie points to.
 */
int
dctl_write_uint32(void *cookie, int len, char *data)
{
	assert(cookie != NULL);
	assert(data != NULL);

	if (len < sizeof(uint32_t)) {
		return(ENOMEM);
	}

	*(uint32_t *)cookie = *(uint32_t *)data;

	return(0);	
}


/*
 * helper function that returns the value of the
 * uint64_t stored at the cookie location.
 */
int
dctl_read_uint64(void *cookie, int *len, char *data)
{

	assert(cookie != NULL);
	assert(data != NULL);

	/* make sure there is a enough space */
	if (*len < sizeof(uint64_t)) {
		*len = sizeof(uint64_t);
		return(ENOMEM);
	}

	/* store the data and the data size */
	*len = sizeof(uint64_t);
	memcpy(data, cookie, sizeof(uint64_t));
	return(0);

}


/*
 * helper function that write the value passed in the data to the
 * uint64_t that the cookie points to.
 */
int
dctl_write_uint64(void *cookie, int len, char *data)
{
	assert(cookie != NULL);
	assert(data != NULL);

	if (len < sizeof(uint64_t)) {
		return(ENOMEM);
	}


	*(uint64_t *)cookie = *(uint64_t *)data;

	return(0);	
}

/*
 * helper function that returns the value of the
 * character stored at the cookie location.
 */

int
dctl_read_char(void *cookie, int *len, char *data)
{

	assert(cookie != NULL);
	assert(data != NULL);

	/* make sure there is a enough space */
	if (*len < sizeof(char)) {
		*len = sizeof(char);
		return(ENOMEM);
	}

		
	/* store the data and the data size */
	*len = sizeof(char);
	*(char *)data = *(char *)cookie;
	return(0);

}


/*
 * helper function that write the value passed in the data to the
 * character that the cookie points to.
 */

int
dctl_write_char(void *cookie, int len, char *data)
{
	assert(cookie != NULL);
	assert(data != NULL);

	if (len < sizeof(char)) {
		return(ENOMEM);
	}

	*(char *)cookie = *(char *)data;
	return(0);	
}


/*
 * helper function that returns the value of the
 * string stored at the cookie location.
 *
 * Note that we do not provide a equivalent write function
 * because of space allocation difficulties.
 */

int
dctl_read_string(void *cookie, int *len, char *data)
{
	int	slen;

	assert(cookie != NULL);
	assert(data != NULL);

	slen = strlen((char *)cookie);
	slen += 1; 	/* include the null term */

	/* make sure there is a enough space */
	if (*len < slen) {
		*len = slen;
		return(ENOMEM);
	}

	/* store the data and the data size */
	*len = slen;
	memcpy((char *)data, (char *)cookie, slen);
	return(0);
}

