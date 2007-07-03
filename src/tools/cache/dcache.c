/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <sys/param.h>

#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_dconfig.h"
#include "lib_odisk.h"
#include "lib_filterexec.h"
#include "lib_ocache.h"

#include "centry.h"

static char const cvsid[] = "$Header$";

/*
 * Read and analyze the contents of a Diamond cache.
 */

#define CACHE_EXT ".CACHEFL"

void usage()
{
	fprintf(stdout, "dcache [-d cache-directory] [-h]\n");
}


int main(int argc, char **argv)
{
	int	c;
	extern char *optarg;
	char *cache_dir;
	char path[PATH_MAX];
	DIR *dir;		
	struct dirent *cur_ent;
	int extlen;
	int flen;
	char *poss_ext;
	int fd;
	cache_obj *cobj;
	cache_init_obj *ciobj;
	
	/* use default cache dir unless told otherwise */
	cache_dir = dconf_get_cachedir();

	/*
	 * The command line options.
	 */
	while (1) {
		c = getopt(argc, argv, "hd:");
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'd':
				cache_dir = optarg;
				break;

			case 'h':
				usage();
				exit(0);
				break;

			default:
				printf("unknown option %c\n", c);
				usage();
				exit(1);
				break;
		}
	}
	
	dir = opendir(cache_dir);
	
	/* dump ATTRSIG file */
	snprintf(path, MAXPATHLEN, "%s/ATTRSIG", cache_dir);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		printf("Could not open %s: %d\n", path, errno);
		exit(1);
	}
	
	while ((ciobj = get_cache_init_entry(fd))) {
		print_cache_init_entry(ciobj);
		free(ciobj);
	}
	close(fd);
	
	/* dump cache entries */
	while ((cur_ent = readdir(dir)) != NULL) {

		/* check for cache file extension */
		extlen = strlen(CACHE_EXT);
		flen = strlen(cur_ent->d_name);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strcmp(poss_ext, CACHE_EXT) != 0) {
				continue;
			}
		} else {
			continue;
		}

		snprintf(path, MAXPATHLEN, "%s/%s", cache_dir, cur_ent->d_name);
		fd = open(path, O_RDONLY);
		if (fd == -1) {
			printf("Could not open %s: %d\n", path, errno);
			continue;
		}
		
		printf("Processing %s\n", path);
		while ((cobj = get_cache_entry(fd))) {
			print_cache_entry(cobj);
			free(cobj);
		}
		close(fd);
	}	
	
	closedir(dir);
	
	exit(0);
}
