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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_odisk.h"
#include "obj_attr.h"


static char const cvsid[] = "$Header$";

int
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

int
show_gid(gid_list_t *glist)
{

	int     i;

	for (i=0; i < glist->num_gids; i++) {
		if (glist->gids[i] != 0) {
			fprintf(stdout, "%016llx\n", glist->gids[i]);
		}
	}
	return(0);
}




int
main(int argc , char **argv)
{
	char *			cur_file;
	char *			poss_ext;
	int             i = 1;
	int			flen;
	int			extlen;
	int             err;
	gid_list_t  *       glist;
	odisk_state_t *		odisk;
	size_t           len;
	obj_data_t  *ohandle;

	odisk_init(&odisk, ".", NULL, NULL);


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
				printf("please use base file !! \n");
				exit(1);
			}
		}

		err = odisk_load_obj(odisk, &ohandle, cur_file);
		assert(err == 0);

		len = 0;
		err = obj_read_attr(&ohandle->attr_info, GIDLIST_NAME, &len, 
					NULL);
		if (err != ENOMEM) {
			fprintf(stderr, "can't get list %d \n", err);
			exit(1);
		}

		glist = malloc(len);
		err = obj_read_attr(&ohandle->attr_info, GIDLIST_NAME, &len, 
			(unsigned char *)glist);
		assert(err == 0);

		show_gid(glist);

	}

	return (0);
}

