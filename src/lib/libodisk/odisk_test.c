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
#include <dirent.h>
#include <stdint.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "odisk_priv.h"

void
remove_some_attr(obj_attr_t *attr, int size, int num)
{
	int	i;
	int	err;
	char	name_string[128];



	for (i=0; i < num; i++) {
		sprintf(name_string, "some_attr_%d_%d", size, i);
		err = obj_del_attr(attr, name_string);
		if (err) {
			printf("failed to del  <%s>\n", name_string);
			exit(1);
		}

	}

}


void
check_some_attr(obj_attr_t *attr, int size, int num)
{
	char *	base_data;
	char *	ret_data;
	int	i;
	int	diff;
	int	err;
	char	name_string[128];
	off_t	attr_size;

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


	for (i=0; i < num; i++) {
		memset(base_data, i, size);
		sprintf(name_string, "some_attr_%d_%d", size, i);
		attr_size = (off_t)(2 * size);
		err = obj_read_attr(attr, name_string, &attr_size, ret_data);
		if (err) {
			printf("failed to read attr <%s>\n", name_string);
			exit(1);
		}

		if (attr_size != size) {
			printf("wrong size on <%s> got %ld want %d \n",
			       name_string, attr_size, size);
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
write_some_attr(obj_attr_t *attr, int size, int num)
{
	char *	data;
	int	i;
	int	err;
	char	name_string[128];

	data = malloc(size);
	if (data == NULL) {
		printf("XXX failed to allocate attributes \n");
		exit(1);
	}


	for (i=0; i < num; i++) {
		memset(data, i, size);
		sprintf(name_string, "some_attr_%d_%d", size, i);
		err = obj_write_attr(attr, name_string, (off_t)size, data);
		if (err) {
			printf("failed to write attr <%s>\n", name_string);
			exit(1);
		}
	}

	free(data);
}


void
test_attrs(obj_attr_t *attr)
{


	write_some_attr(attr, 10, 10);
	check_some_attr(attr, 10, 10);


	write_some_attr(attr, 15, 10);

	check_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);


	write_some_attr(attr, 8099, 10);

	check_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 8099, 10);


	write_some_attr(attr, 4000027, 10);

	check_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 8099, 10);
	check_some_attr(attr, 4000027, 10);


	remove_some_attr(attr, 15, 10);
	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 8099, 10);
	check_some_attr(attr, 4000027, 10);

	write_some_attr(attr, 14, 10);

	check_some_attr(attr, 10, 10);
	check_some_attr(attr, 14, 10);
	check_some_attr(attr, 8099, 10);
	check_some_attr(attr, 4000027, 10);

}


int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	obj_data_t *	new_obj;
	void *		log_cookie;
	void *		dctl_cookie;
	int		err;

	log_init(&log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odisk, "/opt/dir1", dctl_cookie, log_cookie);
	if (err) {
		errno = err;
		perror("failed to init odisk");
		exit(1);
	}
	err = odisk_next_obj(&new_obj, odisk);
	if (err) {
		errno = err;
		perror("failed to get_obj");
		exit(1);
	}

	test_attrs(&new_obj->attr_info);

	printf("test done \n");

	exit(0);
}
