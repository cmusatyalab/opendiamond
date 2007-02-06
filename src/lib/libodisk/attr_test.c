/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "lib_odisk.h"
#include "odisk_priv.h"


static char const cvsid[] =
    "$Header$";

void
remove_some_attr(obj_attr_t * attr, int size, int num)
{
	int             i;
	int             err;
	char            name_string[128];

	for (i = 0; i < num; i++) {
		sprintf(name_string, "some_attr_%d_%d", size, i);
		err = obj_del_attr(attr, name_string);
		if (err) {
			printf("failed to del  <%s>\n", name_string);
			exit(1);
		}

	}

}


void
check_some_attr(obj_attr_t * attr, int size, int num)
{
	char           *base_data;
	unsigned char  *ret_data;
	int             i;
	int             diff;
	int             err;
	char            name_string[128];
	size_t          attr_size;

	base_data = malloc(size);
	if (base_data == NULL) {
		printf("XXX failed to allocate attributes \n");
		exit(1);
	}

	ret_data = malloc(2 * size);
	if (ret_data == NULL) {
		printf("XXX failed to allocate attributes \n");
		exit(1);
	}


	for (i = 0; i < num; i++) {
		memset(base_data, i, size);
		sprintf(name_string, "some_attr_%d_%d", size, i);
		attr_size = (size_t) (2 * size);
		err = obj_read_attr(attr, name_string, &attr_size, ret_data);
		if (err) {
			printf("failed to read attr <%s>\n", name_string);
			exit(1);
		}

		if (attr_size != size) {
			printf("wrong read size <%s>: wanted  %d got %d \n",
			       name_string, size, (int) attr_size);
			exit(1);
		}

		diff = memcmp(base_data, ret_data, size);
		if (diff != 0) {
			printf("objects differ \n");
			exit(1);
		}
	}

	free(base_data);
	free(ret_data);
}

void
write_some_attr(obj_data_t * obj, int size, int num)
{
	unsigned char  *data;
	obj_attr_t     *attr;
	int             i;
	int             err;
	char            name_string[128];

	attr = &obj->attr_info;

	data = malloc(size);
	if (data == NULL) {
		printf("XXX failed to allocate attributes \n");
		exit(1);
	}


	for (i = 0; i < num; i++) {
		memset(data, i, size);
		sprintf(name_string, "some_attr_%d_%d", size, i);
		err = obj_write_attr(attr, name_string, (off_t) size, data);
		if (err) {
			printf("failed to write attr <%s>\n", name_string);
			exit(1);
		}
	}

	free(data);
}

void
write_attr(obj_data_t * obj, int size, char *name)
{
	unsigned char  *data;
	obj_attr_t     *attr;
	int             err;

	attr = &obj->attr_info;

	data = malloc(size);
	if (data == NULL) {
		printf("XXX failed to allocate attributes \n");
		exit(1);
	}

	memset(data, 0xB, size);
	err = obj_write_attr(attr, name, (off_t) size, data);
	if (err) {
		printf("failed to write attr <%s>\n", name);
		exit(1);
	}

	free(data);
}


void
test_attrs(obj_data_t * obj)
{

	obj_attr_t     *attr = &obj->attr_info;

	write_some_attr(obj, 10, 10);
	check_some_attr(attr, 10, 10);


	write_some_attr(obj, 15, 10);

	check_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);


	write_some_attr(obj, 8099, 10);

	check_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 8099, 10);


	write_some_attr(obj, 4000027, 10);

	check_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 8099, 10);
	check_some_attr(attr, 4000027, 10);


	remove_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 8099, 10);
	check_some_attr(attr, 4000027, 10);

	write_some_attr(obj, 14, 10);

	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 14, 10);
	check_some_attr(attr, 8099, 10);
	check_some_attr(attr, 4000027, 10);

}


int
main(int argc, char **argv)
{
	odisk_state_t  *odisk;
	obj_data_t     *new_obj;
	void           *log_cookie;
	void           *dctl_cookie;
	obj_id_t        oid;
	groupid_t       gid;
	int             err;

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, "./test_dir", dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}

	/*
	 * create a new object 
	 */
	gid = 1;
	err = odisk_new_obj(odisk, &oid, &gid);
	assert(err == 0);

	/*
	 * load the object 
	 */
	err = odisk_get_obj(odisk, &new_obj, &oid);
	assert(err == 0);

	/*
	 * write some attrs 
	 */
	write_attr(new_obj, 400001, "test_attr");


	err = odisk_save_obj(odisk, new_obj);
	assert(err == 0);

	odisk_release_obj(new_obj);


	/*
	 * reload the object, not the size, and delete the
	 * attributes and resave them.
	 */
	err = odisk_get_obj(odisk, &new_obj, &oid);
	assert(err == 0);

	obj_del_attr(&new_obj->attr_info, "test_attr");

	err = odisk_save_obj(odisk, new_obj);
	assert(err == 0);

	odisk_release_obj(new_obj);


	/*
	 * reload and make sure our size is now less than before 
	 */
	err = odisk_get_obj(odisk, &new_obj, &oid);
	assert(err == 0);

	/*
	 * save the object 
	 */
	exit(0);
}
