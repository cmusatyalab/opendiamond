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


#include    <sys/types.h>
#include    <unistd.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <stdint.h>
#include    <assert.h>
#include    "diamond_types.h"
#include    "lib_dconfig.h"


#define MAX_GIDS    64

int
enum_names()
{
	void           *cookie;
	char           *name;
	int             err;

	err = nlkup_first_entry(&name, &cookie);
	assert(err == 0);
	printf("name:  %s \n", name);

	while (1) {
		err = nlkup_next_entry(&name, &cookie);
		if (err) {
			break;
		}
		printf("name:  %s \n", name);
	}

	return (0);
}

void
dump_gid_list()
{
	void           *cookie;
	char           *name;
	int             err;
	int             num, i;
	groupid_t       gid_list[MAX_GIDS];
	num = MAX_GIDS;


	err = nlkup_first_entry(&name, &cookie);
	assert(err == 0);

	err = nlkup_lookup_collection(name, &num, gid_list);
	assert(err == 0);

	fprintf(stdout, "%s: ", name);
	for (i = 0; i < num; i++) {
		fprintf(stdout, "%016llx ", gid_list[i]);
	}
	fprintf(stdout, "\n");


	while (1) {
		err = nlkup_next_entry(&name, &cookie);
		if (err) {
			break;
		}

		err = nlkup_lookup_collection(name, &num, gid_list);
		assert(err == 0);
		fprintf(stdout, "%s: ", name);
		for (i = 0; i < num; i++) {
			fprintf(stdout, "%016llx ", gid_list[i]);
		}
		fprintf(stdout, "\n");
	}

}


int
main(int argc, char **argv)
{
	groupid_t       gid_list[MAX_GIDS];
	int             num, err, i;

	err = enum_names();
	dump_gid_list();

	num = MAX_GIDS;
	err = nlkup_lookup_collection("foo", &num, gid_list);
	if (err) {
		fprintf(stdout, "failed lookup on <foo> \n");
	} else {
		fprintf(stdout, "foo ");
		for (i = 0; i < num; i++) {
			printf("%016llx ", gid_list[i]);
		}
		fprintf(stdout, "\n");
	}
#ifdef  XXX
	gid_list[0] = 1;
	err = lookup_add_name("foo1", 1, gid_list);
	assert(err == 0);
#endif


	exit(0);
}
