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
#include <dirent.h>
#include "sys_attr.h"
#include "obj_attr.h"


int
set_defattr(char *attr_name, char *data_name)
{
	struct stat	stats;
	int		err;
	obj_attr_t	attr;


	err = stat(data_name, &stats);
	if (err != 0) {
		perror("failed to open file");
		exit(1);
	}

	err = obj_read_attr_file(attr_name, &attr);
	if (err != 0) {
		printf("XXX failed to init attr \n");
		exit(1);
	}

	err = obj_write_attr(&attr, SIZE, sizeof(stats.st_size),
	                     (char *)&stats.st_size);
	if (err) {
		printf("failed to write size \n");
		exit(1);
	}

	err = obj_write_attr(&attr, UID, sizeof(stats.st_uid),
	                     (char *)&stats.st_uid);
	if (err) {
		printf("failed to write uid \n");
		exit(1);
	}

	err = obj_write_attr(&attr, GID, sizeof(stats.st_gid),
	                     (char *)&stats.st_gid);
	if (err) {
		printf("failed to write gid \n");
		exit(1);
	}

	err = obj_write_attr(&attr, BLK_SIZE, sizeof(stats.st_blksize),
	                     (char *)&stats.st_blksize);
	if (err) {
		printf("failed to write blk_size \n");
		exit(1);
	}

	err = obj_write_attr(&attr, ATIME, sizeof(stats.st_atime),
	                     (char *)&stats.st_atime);
	if (err) {
		printf("failed to write atime \n");
		exit(1);
	}


	err = obj_write_attr(&attr, MTIME, sizeof(stats.st_mtime),
	                     (char *)&stats.st_mtime);
	if (err) {
		printf("failed to write mtime \n");
		exit(1);
	}

	err = obj_write_attr(&attr, CTIME, sizeof(stats.st_ctime),
	                     (char *)&stats.st_ctime);
	if (err) {
		printf("failed to write ctime \n");
		exit(1);
	}


	/*
	 * Write out the modified attributes.
	 */
	err = obj_write_attr_file(attr_name, &attr);
	if (err != 0) {
		printf("XXX failed to write attributes \n");
		exit(1);
	}


	return (0);
}



int
main(int argc , char **argv)
{
	int			i = 1;
	char			attr_name[NAME_MAX];
	char			base_name[NAME_MAX];
	char *			cur_file;
	char *			poss_ext;
	int			flen;
	int			extlen;
	int			is_attr = 0;

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
			strncpy(base_name, cur_file, (flen - extlen));
			base_name[flen - extlen] = '\0';
			strcpy(attr_name, cur_file);

		} else {
			sprintf(attr_name, "%s%s", cur_file, ATTR_EXT);
			strcpy(base_name, cur_file);
		}

		printf("file name <%s> \n", base_name);
		printf("attr name <%s> \n", attr_name);
		set_defattr(attr_name, base_name);
	}

	return (0);
}

