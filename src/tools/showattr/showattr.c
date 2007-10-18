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
#include <errno.h>
#include <string.h>
#include <dirent.h>
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


static int
isstring(char *str, int len)
{
	int i;
	for (i = 0; i<(len -1); i++) {
		if (!(isalnum((int)str[i])||isspace((int)str[i]))) {
			/*	printf("isstring : %c \n", str[i]); */
			return(0);
		}
	}

	if (str[len-1] != '\0') {
		/* printf("isstring : not null \n"); */
		return(0);
	}
	return(1);

}

static int
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


static int
show_attr(odisk_state_t *odisk, char *attr_name)
{
	int		err;
	obj_attr_t 	attr;
	attr_record_t	*cur_rec = NULL;
	obj_adata_t *		adata;
	int		cur_offset;

	err = obj_read_attr_file(odisk, attr_name, &attr);
	if (err != 0) {
		printf("XXX failed to init attr \n");
		exit(1);
	}

	/*
	 * walk through all the attributes records and
	 * display them.
	 */

	for (adata = attr.attr_dlist; adata != NULL; adata = adata->adata_next) {
		cur_offset = 0;
		while (cur_offset < adata->adata_len) {
			cur_rec = (attr_record_t *)&adata->adata_data[cur_offset];
			print_attr(cur_rec);
			cur_offset += cur_rec->rec_len;
		}
	}

	return (0);
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
	odisk_state_t *	odisk;
	int		err;

        log_init("showattr", NULL);

        err = odisk_init(&odisk, NULL);
        if (err) {
                errno = err;
                perror("failed to init odisk");
                exit(1);
        }



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

		show_attr(odisk, attr_name);
	}

	log_term();
	return (0);
}

