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
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include "lib_dctl.h"

static char const cvsid[] =
    "$Header$";

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
		return (ENOMEM);
	}


	*len = sizeof(uint32_t);
	*(uint32_t *) data = *(uint32_t *) cookie;

	return (0);
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
		return (ENOMEM);
	}

	*(uint32_t *) cookie = *(uint32_t *) data;

	return (0);
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

	/*
	 * make sure there is a enough space 
	 */
	if (*len < sizeof(uint64_t)) {
		*len = sizeof(uint64_t);
		return (ENOMEM);
	}

	/*
	 * store the data and the data size 
	 */
	*len = sizeof(uint64_t);
	memcpy(data, cookie, sizeof(uint64_t));
	return (0);

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
		return (ENOMEM);
	}


	*(uint64_t *) cookie = *(uint64_t *) data;

	return (0);
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

	/*
	 * make sure there is a enough space 
	 */
	if (*len < sizeof(char)) {
		*len = sizeof(char);
		return (ENOMEM);
	}


	/*
	 * store the data and the data size 
	 */
	*len = sizeof(char);
	*(char *) data = *(char *) cookie;
	return (0);

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
		return (ENOMEM);
	}

	*(char *) cookie = *(char *) data;
	return (0);
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
	int             slen;

	assert(cookie != NULL);
	assert(data != NULL);

	slen = strlen((char *) cookie);
	slen += 1;		/* include the null term */

	/*
	 * make sure there is a enough space 
	 */
	if (*len < slen) {
		*len = slen;
		return (ENOMEM);
	}

	/*
	 * store the data and the data size 
	 */
	*len = slen;
	memcpy((char *) data, (char *) cookie, slen);
	return (0);
}
