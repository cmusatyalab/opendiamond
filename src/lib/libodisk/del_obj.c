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
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_odisk.h"
#include "odisk_priv.h"



uint64_t
parse_uint64_string(const char* s)
{
	int i, o;
	unsigned int x;   // Will actually hold an unsigned char
	uint64_t u = 0u;

	/*
	sscanf(s, "%llx", &u);
	printf("parsed gid is 0x%llx\n", u);
	return u;
	*/

	assert(s);
	//fprintf(stderr, "parse_uint64_string s = %s\n", s);
	for (i=0; i<8; i++) {
		o = 3*i;
		assert(isxdigit(s[o]) && isxdigit(s[o+1]));
		assert( (s[o+2] == ':') || (s[o+2] == '\0') );
		sscanf(s+o, "%2x", &x);
		u <<= 8;
		u += x;
	}
	// printf("parsed uint64_t is 0x%llx\n", u);
	return u;
}





int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	obj_data_t *	new_obj;
	void *		log_cookie;
	void *		dctl_cookie;
	obj_id_t	oid;
	int		err;

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
