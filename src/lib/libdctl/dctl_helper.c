/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
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

#include "dctl_impl.h"

/*
 * helper function that returns the value of the
 * uint32_t stored at the cookie location.
 */
static int
dctl_read_uint32(void *cookie, size_t *len, char *data)
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
static int
dctl_write_uint32(void *cookie, size_t len, char *data)
{
	assert(cookie != NULL);
	assert(data != NULL);

	if (len < sizeof(uint32_t)) {
		return (ENOMEM);
	}

	*(uint32_t *) cookie = *(uint32_t *) data;

	return (0);
}

void dctl_register_u32(char *path, char *leaf, int mode, uint32_t *item)
{
    dctl_read_fn read   = mode != O_WRONLY ? dctl_read_uint32 : NULL;
    dctl_write_fn write = mode != O_RDONLY ? dctl_write_uint32 : NULL;
    int err;

    err = dctl_register_leaf(path, leaf, DCTL_DT_UINT32, read, write, item);
    assert(err == 0);
}


/*
 * helper function that returns the value of the
 * uint64_t stored at the cookie location.
 */
static int
dctl_read_uint64(void *cookie, size_t *len, char *data)
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
static int
dctl_write_uint64(void *cookie, size_t len, char *data)
{
	assert(cookie != NULL);
	assert(data != NULL);

	if (len < sizeof(uint64_t)) {
		return (ENOMEM);
	}

	*(uint64_t *) cookie = *(uint64_t *) data;

	return (0);
}

void dctl_register_u64(char *path, char *leaf, int mode, uint64_t *item)
{
    dctl_read_fn  read  = mode != O_WRONLY ? dctl_read_uint64 : NULL;
    dctl_write_fn write = mode != O_RDONLY ? dctl_write_uint64 : NULL;
    int err;

    err = dctl_register_leaf(path, leaf, DCTL_DT_UINT64, read, write, item);
    assert(err == 0);
}


