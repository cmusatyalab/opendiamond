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
show_attr(char *attr_name)
{	
	int		err;
	obj_attr_t 	attr;
	attr_record_t	*cur_rec = NULL;
	int		cur_offset;

	err = obj_read_attr_file(attr_name, &attr);
	if (err != 0) {
		printf("XXX failed to init attr \n");
		exit(1);
	}

	/*
	 * walk through all the attributes records and
	 * display them.
	 */
	cur_offset = 0;
	while (cur_offset < attr.attr_len) {
		cur_rec = (attr_record_t *)&attr.attr_data[cur_offset];
		print_attr(cur_rec);
		cur_offset += cur_rec->rec_len;
	}

	return (0);
}



int
main(int argc , char **argv) 
{
	int			i = 1;
	char			attr_name[MAX_ATTR_NAME];
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
			strcpy(attr_name, cur_file);
		} else {
			sprintf(attr_name, "%s%s", cur_file, ATTR_EXT);
		}

		show_attr(attr_name);
	}

	return (0);
}

