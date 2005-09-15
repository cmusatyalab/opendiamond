/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <unistd.h>
#include "lib_tools.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "odisk_priv.h"


static char const cvsid[] = "$Header$";

uint64_t
parse_uint64_string(const char* s)
{
	int i, o;
	unsigned int x;	// Will actually hold an unsigned char
	uint64_t u = 0u;

	/*
	sscanf(s, "%llx", &u);
	printf("parsed gid is 0x%llx\n", u);
	return u;
	*/

	assert(s);
	//fprintf(stderr, "parse_uint64_string s = %s\n", s);
	for (i=0; i<8; i++) {
		o = 3*i;
		assert(isxdigit(s[o]) && isxdigit(s[o+1]));
		assert( (s[o+2] == ':') || (s[o+2] == '\0') );
		sscanf(s+o, "%2x", &x);
		u <<= 8;
		u += x;
	}
	// printf("parsed uint64_t is 0x%llx\n", u);
	return u;
}


void
usage()
{
	fprintf(stdout, "group_count [-c] -g <gid> [-m <max_objs>] \n");
	fprintf(stdout, "\t-c show count of objects in the group \n");
	fprintf(stdout, "\t-g <gid> gid of the group to modify \n");
	fprintf(stdout, "\t-m <max_objs> keep the first max_objs of the group \n");
}



int
main(int argc, char **argv)
{
	odisk_state_t*	odisk;
	FILE * 		cur_file;
	char		idx_file[256];
	char		path_name[256];
	char		attr_name[256];
	char *		path = "/opt/dir1";
	int			max = 0;
	int			have_gid = 0;
	gid_idx_ent_t	gid_ent;
	uint64_t	gid = 0;
	int			err, num;
	int			i,c;
	int			do_count = 0;
	int			count = 0;
	uint64_t		data_size = 0;
	uint64_t		attr_size = 0;
	uint64_t		total_size = 0;
	struct stat		sbuf;
	extern char *	optarg;

	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "chg:m:");
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'h':
				usage();
				exit(0);
				break;


			case 'g':
				gid = parse_uint64_string(optarg);
				have_gid = 1;
				break;

			default:
				printf("unknown option %c\n", c);
				break;
		}
	}

	if (have_gid == 0) {
		usage();
		exit(1);
	}


	sprintf(idx_file, "%s/%s%016llX", path, GID_IDX, gid);
	cur_file = fopen(idx_file, "r");
	if (cur_file == NULL) {
		fprintf(stderr, "unable to open idx %s \n", idx_file);
	}



	while (1) {
		num = fread(&gid_ent, sizeof(gid_ent), 1, cur_file);
		if (num == 1) {
			sprintf(path_name, "%s/%s", path, gid_ent.gid_name);
			err = stat(path_name, &sbuf);
			assert(err == 0);
			data_size += sbuf.st_size;
			total_size += sbuf.st_size;

			sprintf(attr_name, "%s%s", path_name, BIN_ATTR_EXT);
			err = stat(attr_name, &sbuf);
			assert(err == 0);
			attr_size += sbuf.st_size;
			total_size += sbuf.st_size;

		} else {
			goto done;

		}
	}

done:
	fprintf(stdout,"total %lld attr %lld data %lld \n",
	        total_size, attr_size, data_size);
	fclose(cur_file);
	exit(0);
}
