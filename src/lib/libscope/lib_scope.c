/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2007 Carnegie Mellon University
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
#include <sys/param.h>
#include "lib_scope.h"

#define INT_CHARSIZE 16

int
ls_define_scope(void) {
  FILE *fp = NULL, *np = NULL, *gp = NULL;
  char ns[INT_CHARSIZE], gs[INT_CHARSIZE], path[MAXPATHLEN], *user;
  unsigned int namemap_size, gidmap_size;
  int i;

  if((user = getenv("USER")) == NULL) {
	fprintf(stderr, "Couldn't get user!\n");
	return -1;
  }

  snprintf(path, MAXPATHLEN, "/home/%s/.diamond/NEWSCOPE", user);
  if((fp = fopen(path, "r")) == NULL) {
	fprintf(stderr, "couldn't open %s!\n", path);
	goto exit_failure;
  }

  snprintf(path, MAXPATHLEN, "/home/%s/.diamond/name_map", user);
  if((np = fopen(path, "w")) == NULL) {
	fprintf(stderr, "couldn't open %s!\n", path);
	goto exit_failure;
  }

  snprintf(path, MAXPATHLEN, "/home/%s/.diamond/gid_map", user);
  if((gp = fopen(path, "w")) == NULL) {
	fprintf(stderr, "couldn't open %s!\n", path);
	goto exit_failure;
  }


  /* Read name_map size and data. */

  if(fgets(ns, INT_CHARSIZE, fp) == NULL) {
	fprintf(stderr, "couldn't read name_map size\n");
	goto exit_failure;
  }
  else {
	int len = strlen(ns);
	ns[len-1]='\0';  //remove '\n'
  }

  namemap_size = atoi(ns);

  for(i=0; i<namemap_size; i++) {
	char line[NCARGS];

	if(fgets(line, NCARGS, fp) == NULL) {
	  fprintf(stderr, "couldn't read name_map data\n");
	  goto exit_failure;
	}

	if((fwrite(line, strlen(line), 1, np)) != 1) {
	  fprintf(stderr, "couldn't write name_map data\n");
	  goto exit_failure;
	}
  }


  /* Read gid_map size and data. */

  if(fgets(gs, INT_CHARSIZE, fp) == NULL) {
	fprintf(stderr, "couldn't read gid_map size\n");
	goto exit_failure;
  }
  else {
	int len = strlen(gs);
	gs[len-1]='\0';  //remove '\n'
  }

  gidmap_size = atoi(gs);

for(i=0; i<gidmap_size; i++) {
	char line[NCARGS];

	if(fgets(line, NCARGS, fp) == NULL) {
	  fprintf(stderr, "couldn't read name_map data\n");
	  goto exit_failure;
	}

	if((fwrite(line, strlen(line), 1, gp)) != 1) {
	  fprintf(stderr, "couldn't write name_map data\n");
	  goto exit_failure;
	}
  }


  if(fp) fclose(fp);
  if(np) fclose(np);
  if(gp) fclose(gp);

  return 0;


 exit_failure:

  if(fp) fclose(fp);
  if(np) fclose(np);
  if(gp) fclose(gp);

  return -1;
}
