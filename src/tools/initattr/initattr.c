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
#include "attr.h"
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

