#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
#include <assert.h>
#include "lib_dctl.h"

int	var = 1;

#define	MAX_ENTS	128
void
dump_node(char *cur_path)
{

	dctl_entry_t	data[MAX_ENTS];
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
		len = sizeof(tmp);
		err = dctl_read_leaf("foo.var",  &len, (char *)&tmp);
		if (err) {
			printf("failed read on \n");
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


void
simple_test()
{
	int err;
	uint32_t	tmp;
	int		len;
	dump_tree();

	err = dctl_register_node("", "foo");
	assert(err == 0);
	err = dctl_register_node("", "bar");
	assert(err == 0);
	err = dctl_register_node("foo", "dir1");
	assert(err == 0);


	err = dctl_register_leaf("foo.dir1", "var1", DCTL_DT_UINT32, 
			dctl_read_uint32, dctl_write_uint32, &var);
	assert(err == 0);

	err = dctl_register_leaf("foo.dir1", "var2", DCTL_DT_UINT32, 
			dctl_read_uint32, dctl_write_uint32, &var);
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
	dctl_read_leaf("foo.var",  &len, (char *)&tmp);
	tmp = 21;
	len = sizeof(tmp);
	dctl_write_leaf("foo.var",  len, (char *)&tmp);
	tmp = 0;
	len = sizeof(tmp);
	dctl_read_leaf("foo.var",  &len, (char *)&tmp);
	assert(tmp == 21);



	dump_tree();

	dctl_unregister_node("", "foo");
	dctl_unregister_node("", "bar");


	dump_tree();

}


int
main(int argc, char **argv)
{

	dctl_init();

	simple_test();

	return(0);
}
