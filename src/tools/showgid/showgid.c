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
#include "lib_odisk.h"
#include "obj_attr.h"


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
	off_t           len;
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
		extlen = strlen(ATTR_EXT);
		flen = strlen(cur_file);

		/* XXX check maxlen !! */
		if (flen > extlen) {
			poss_ext = &cur_file[flen - extlen];
			if (strcmp(poss_ext, ATTR_EXT) == 0) {
				printf("please use base file !! \n");
				exit(1);
			}
		}

		err = odisk_load_obj(odisk, &ohandle, cur_file);
		assert(err == 0);

		len = 0;
		err = obj_read_attr(&ohandle->attr_info, GIDLIST_NAME, &len, NULL);
		if (err != ENOMEM) {
			fprintf(stderr, "can't get list %d \n", err);
			exit(1);
		}

		glist = malloc(len);
		err = obj_read_attr(&ohandle->attr_info, GIDLIST_NAME, &len, (char *)glist);
		assert(err == 0);

		show_gid(glist);

	}

	return (0);
}

