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
#include <netinet/in.h>
#include <assert.h>
#include <string.h>
#include "lib_dctl.h"

int	var = 1;

#define	MAX_ENTS	128
void
dump_node(char *cur_path)
{

	dctl_entry_t	    data[MAX_ENTS];
	dctl_data_type_t    dtype;
	char                leaf[128];
	int		ents;
	int		err, i;
	int		len;
	uint32_t	tmp;


	/*
	 * first all the leafs.
	 */
	ents = MAX_ENTS;
	err = dctl_list_leafs(cur_path, &ents, data);
	if (err) {
		printf("err on list lisf %d %s \n", err, cur_path);
		exit(1);
	}

	for (i = 0; i < ents; i++) {
		fprintf(stdout, "%s.%s ", cur_path, data[i].entry_name);
		sprintf(leaf, "%s.%s", cur_path, data[i].entry_name);
		len = sizeof(tmp);
		err = dctl_read_leaf(leaf,  &dtype, &len, (char *)&tmp);
		if (err) {
			printf("failed to read <%s> on %d \n", leaf, err);
			exit(1);
		}
		printf("= %d \n", tmp);

	}

	ents = MAX_ENTS;
	err = dctl_list_nodes(cur_path, &ents, data);
	assert(err == 0);

	for (i = 0; i < ents; i++) {
		char	new_path[256];
		fprintf(stdout, "%s.%s.\n", cur_path, data[i].entry_name);
		sprintf(new_path, "%s.%s", cur_path, data[i].entry_name);
		dump_node(new_path);
	}
}

void
dump_tree()
{
	dctl_entry_t	data[MAX_ENTS];
	int		ents;
	int		err, i;

	ents = MAX_ENTS;
	err = dctl_list_nodes("", &ents, data);
	assert(err == 0);

	for (i = 0; i < ents; i++) {
		char	new_path[256];
		fprintf(stdout, "%s.\n", data[i].entry_name);
		sprintf(new_path, "%s", data[i].entry_name);
		dump_node(new_path);
	}

}



int
remote_write_leaf(char *path, int len, char *data, void *cookie)
{
	printf("write_leaf:  <%s> \n", path);
	return(0);
}

int
remote_read_leaf(char *path, dctl_data_type_t *dtype, int *len, char *data, void *cookie)
{
	printf("read_leaf:  <%s> \n", path);
	*dtype = DCTL_DT_UINT32;
	*(uint32_t *)data = 57;
	*len = sizeof(uint32_t);
	return(0);
}


int
remote_list_nodes(char *path, int *num_ents, dctl_entry_t *space,
                  void *cookie)
{
	printf("list_nodes:  <%s> \n", path);
	*num_ents = 0;
	return(0);
}


int
remote_list_leafs(char *path, int *num_ents, dctl_entry_t *space,
                  void *cookie)
{
	printf("list_leafs:  <%s> %p\n", path, path);

	strcpy(space[0].entry_name, "test");
	space[0].entry_type = DCTL_DT_UINT32;
	*num_ents = 1;
	return(0);
}



void
simple_test()
{
	int                 err;
	uint32_t	        tmp;
	int	                len;
	dctl_fwd_cbs_t      cbs;
	dctl_data_type_t    dtype;
	dump_tree();

	err = dctl_register_node("", "foo");
	assert(err == 0);
	err = dctl_register_node("", "bar");
	assert(err == 0);
	err = dctl_register_node("foo", "dir1");
	assert(err == 0);

	err = dctl_register_node("foo.dir1", "before");
	assert(err == 0);

	err = dctl_register_leaf("foo.dir1", "var1", DCTL_DT_UINT32,
	                         dctl_read_uint32, dctl_write_uint32, &var);
	assert(err == 0);

	err = dctl_register_leaf("foo.dir1", "var2", DCTL_DT_UINT32,
	                         dctl_read_uint32, dctl_write_uint32, &var);

	cbs.dfwd_rleaf_cb = remote_read_leaf;
	cbs.dfwd_wleaf_cb = remote_write_leaf;
	cbs.dfwd_lnodes_cb = remote_list_nodes;
	cbs.dfwd_lleafs_cb = remote_list_leafs;
	cbs.dfwd_cookie = 0;

	err = dctl_register_fwd_node("foo.dir1", "fwd", &cbs);
	if (err) {
		printf("register fwd failed \n");
		exit(1);
	}

	err = dctl_register_node("foo.dir1", "after");
	assert(err == 0);


	assert(err == 0);
	err = dctl_register_leaf("foo.dir1", "var3", DCTL_DT_UINT32,
	                         dctl_read_uint32, dctl_write_uint32, &var);
	assert(err == 0);
	dctl_register_leaf("foo.dir1", "var4", DCTL_DT_UINT32,
	                   dctl_read_uint32, dctl_write_uint32, &var);
	assert(err == 0);


	dctl_register_leaf("foo", "var", DCTL_DT_UINT32,
	                   dctl_read_uint32, dctl_write_uint32, &var);

	len = sizeof(tmp);
	dctl_read_leaf("foo.var",  &dtype, &len, (char *)&tmp);
	tmp = 21;
	len = sizeof(tmp);
	dctl_write_leaf("foo.var",  len, (char *)&tmp);
	tmp = 0;
	len = sizeof(tmp);
	dctl_read_leaf("foo.var",  &dtype, &len, (char *)&tmp);
	assert(tmp == 21);



	dump_tree();

	dctl_unregister_node("", "foo");
	dctl_unregister_node("", "bar");


	dump_tree();
}


int
main(int argc, char **argv)
{
	void *cookie;

	dctl_init(&cookie);

	simple_test();

	return(0);
}
