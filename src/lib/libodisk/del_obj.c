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
#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_odisk.h"
#include "odisk_priv.h"

static char const cvsid[] =
    "$Header$";

uint64_t
parse_uint64_string(const char *s)
{
	int             i,
	                o;
	unsigned int    x;	// Will actually hold an unsigned char
	uint64_t        u = 0u;

	/*
	 * sscanf(s, "%llx", &u); printf("parsed gid is 0x%llx\n", u); return 
	 * u; 
	 */

	assert(s);
	// fprintf(stderr, "parse_uint64_string s = %s\n", s);
	for (i = 0; i < 8; i++) {
		o = 3 * i;
		assert(isxdigit(s[o]) && isxdigit(s[o + 1]));
		assert((s[o + 2] == ':') || (s[o + 2] == '\0'));
		sscanf(s + o, "%2x", &x);
		u <<= 8;
		u += x;
	}
	// printf("parsed uint64_t is 0x%llx\n", u);
	return u;
}





int
main(int argc, char **argv)
{
	odisk_state_t  *odisk;
	obj_data_t     *new_obj;
	void           *log_cookie;
	void           *dctl_cookie;
	obj_id_t        oid;
	int             err;

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, "test_dir", dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	assert(argc == 2);

	oid.dev_id = 0;
	oid.local_id = parse_uint64_string(argv[1]);
	printf("id %llX \n", oid.local_id);

	err = odisk_get_obj(odisk, &new_obj, &oid);
	assert(err == 0);


	odisk_delete_obj(odisk, new_obj);

	exit(0);
}
