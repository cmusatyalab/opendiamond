/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * This library provides the main functions of the dynamic
 * metadata scoping API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <unistd.h>
#include "lib_scope.h"


#define MAX_ROTATE_FILENO 9

/* rotate a set of files */
static void rotate(const char *path)
{
    char buf[2][MAXPATHLEN], *cur, *prev = NULL;
    unsigned int i;

    for (i = MAX_ROTATE_FILENO; i > 0; i--)
    {
	cur = buf[i % 2];
	snprintf(cur, MAXPATHLEN, "%s-%u", path, i);
	if (prev) rename(cur, prev);
	prev = cur;
    }
    rename(path, prev);
}

static int copy_mapdata(const char *name, FILE *fp)
{
    char line[NCARGS];
    unsigned int i, nlines;
    size_t len;
    int out, err;

    /* Read nlines. */
    err = fscanf(fp, "%u\n", &nlines);
    if (err != 1) {
	fprintf(stderr, "couldn't read %s size\n", name);
	return -1;
    }

    /* Copy data. */
    out = open(name, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (out == -1) {
	fprintf(stderr, "couldn't open %s for writing\n", name);
	return -1;
    }

    err = 0;
    for (i = 0; i < nlines; i++) {
	if (fgets(line, NCARGS, fp) == NULL) {
	    fprintf(stderr, "couldn't read map data for %s\n", name);
	    err = -1;
	    break;
	}

	len = strlen(line);
	if (write(out, line, len) != (ssize_t)len) {
	    fprintf(stderr, "couldn't write map data for %s\n", name);
	    err = -1;
	    break;
	}
    }

    close(out);
    return err;
}

/* Parse the NEWSCOPE file and write out name_map and gid_map files for the
 * Diamond application to read. */
int ls_define_scope(void)
{
    FILE *fp;
    char path[MAXPATHLEN], *home;
    int err;

    home = getenv("HOME");
    if (!home) {
	fprintf(stderr, "libscope: Couldn't get user's home directory!\n");
	return -1;
    }

    snprintf(path, MAXPATHLEN, "%s/.diamond/NEWSCOPE", home);
    fp = fopen(path, "r");
    if (fp == NULL) {
	fprintf(stderr, "Couldn't open scope file %s!\n", path);
	return 0;
    }

    /* Rotate old name_map files out of the way and write out the new file. */
    snprintf(path, MAXPATHLEN, "%s/.diamond/name_map", home);
    rotate(path);
    err = copy_mapdata(path, fp);
    if (err) goto exit;

    /* Rotate old gid_map files out of the way and write out the new file. */
    snprintf(path, MAXPATHLEN, "%s/.diamond/gid_map", home);
    rotate(path);
    err = copy_mapdata(path, fp);
    if (err) goto exit;

exit:
    fclose(fp);
    return err;
}
