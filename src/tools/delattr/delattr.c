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
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "obj_attr.h"


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
	char *data;

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

	if (isstring(data, arec->data_len)) {
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
del_attr(char *attr_fname, char *aname)
{	
	int		err;
	obj_attr_t 	attr;

	err = obj_read_attr_file(attr_fname, &attr);
	if (err != 0) {
		printf("XXX failed to init attr \n");
		exit(1);
	}


	err = obj_del_attr(&attr, aname);
	if (err != 0) {
		printf("XXX failed to find attr !! \n");
		exit(1);
	}

	err = obj_write_attr_file(attr_fname, &attr);
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
	int			i;
	char			attr_name[NAME_MAX];
	char *			cur_file;
	char *			poss_ext;
	int			flen;
	int			extlen;
	int			is_attr = 0;
	char *			aname;


	aname = argv[1];

	i = 2;
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
				is_attr = 1;
			}
		}

		if (is_attr) {
			strcpy(attr_name, cur_file);

		} else {
			sprintf(attr_name, "%s%s", cur_file, ATTR_EXT);
		}

		del_attr(attr_name, aname);
	}

	return (0);
}

