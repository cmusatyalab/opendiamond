/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_dconfig.h"
#include "obj_attr.h"


static char const cvsid[] = "$Header$";

int
isstring(char *str, int len)
{
	int i;
	for (i = 0; i<(len -1); i++) {
		if (!(isalnum((int)str[i]))) {
			return(0);
		}
	}

	if (str[len-1] != '\0') {
		return(0);
	}
	return(1);

}

int
print_attr(attr_record_t *arec)
{
	unsigned char *data;

	if (arec->flags & ATTR_FLAG_FREE) {
		printf("%-20s ", " ");
		printf(" F ");
		printf("%8d \n", arec->rec_len);
		return(0);
	} else {
		printf("%-20s ", &arec->data[0]);
		printf("   ");
		printf("%8d ", arec->data_len);
	}

	data = &arec->data[arec->name_len];

	if (isstring((char *)data, arec->data_len)) {
		printf("%s \n", data);
	} else {
		if (arec->data_len == 4) {
			int	foo = *(int *)data;
			printf("%16d \n", foo);

		} else {
			int i;

			/* XXX this messes up the endian stuff for little */
			printf("0X");
			for (i=0; i < arec->data_len; i++) {
				printf("%02x", (unsigned char)data[i]);
			}
			printf("\n");
		}
	}
	return(0);
}


int
add_attr(odisk_state_t *odisk, char *attr_name, char *aname, 
	unsigned char *data, int datalen)
{
	int		err;
	obj_attr_t 	attr;

	err = obj_read_attr_file(odisk, attr_name, &attr);
	if (err != 0) {
		printf("XXX failed to init attr \n");
		exit(1);
	}


	err = obj_write_attr(&attr, aname, datalen, data);
	if (err) {
		printf("failed to write attributes \n");
		exit(1);
	}


	err = obj_write_attr_file(attr_name, &attr);
	if (err != 0) {
		printf("XXX failed to write attributes \n");

	}
	return (0);
}

void
usage()
{
	printf("setattr attribute [-v value][-s string] file1 <file2 ...>\n");

}

int
main(int argc , char **argv)
{
	int			i = 1;
	char			attr_name[NAME_MAX];
	char *			cur_file;
	char *			poss_ext;
	int			flen;
	int			extlen;
	int			is_attr = 0;
	char *			aname;
	unsigned char *		data;
	int			datalen;
	int			value;
	int			err;
	void *			dctl_cookie;
	void *			log_cookie;
	odisk_state_t *		odisk;

	dctl_init(&dctl_cookie);
        log_init("setattr", NULL, &log_cookie);
        err = odisk_init(&odisk, NULL, dctl_cookie, log_cookie);
        if (err) {
                errno = err;
                perror("failed to init odisk");
                exit(1);
        }

	if (argc < 3) {
		usage();
		exit(1);
	}

	if (argv[2][0]!= '-') {
		usage();
		exit(1);
	}

	aname = argv[1];

	switch  (argv[2][1]) {
		case 's':
			data = (unsigned char *)argv[3];
			datalen = strlen((char *)data) + 1;
			break;

		case 'v':
			value = strtol(argv[3], NULL, 0);
			datalen = sizeof(value);
			data = (unsigned char *)&value;
			break;

		default:
			usage();
			exit(1);
			break;
	}

	i = 4;
	while (argc != i) {
		cur_file = argv[i];
		i++;

		/*
		 * if the name ends in ".attr" then we were passed
		 * the attribute file and we need to determine the real
		 * file name.
		 */
		extlen = strlen(BIN_ATTR_EXT);
		flen = strlen(cur_file);

		/* XXX check maxlen !! */
		if (flen > extlen) {
			poss_ext = &cur_file[flen - extlen];
			if (strcmp(poss_ext, BIN_ATTR_EXT) == 0) {
				is_attr = 1;
			}
		}

		if (is_attr) {
			strcpy(attr_name, cur_file);

		} else {
			sprintf(attr_name, "%s%s", cur_file, BIN_ATTR_EXT);
		}

		add_attr(odisk, attr_name, aname, data, datalen);
	}

	log_term(log_cookie);
	return (0);
}

