/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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
  FILE *fp, *np = NULL, *gp = NULL, *rot;
  char ns[INT_CHARSIZE], gs[INT_CHARSIZE], path[MAXPATHLEN], *home;
  unsigned int namemap_size, gidmap_size;
  int i;


  if((home = getenv("HOME")) == NULL) {
    fprintf(stderr, "libscope: Couldn't get user's home directory!\n");
    return -1;
  }


  /* If there is no NEWSCOPE file, punt early and don't screw up
     the rotated map file history. */

  snprintf(path, MAXPATHLEN, "%s/.diamond/NEWSCOPE", home);
  if((fp = fopen(path, "r")) == NULL) {
    fprintf(stderr, "libscope: Couldn't open %s!\n", path);
    return 0;
  }
  fclose(fp); fp = NULL;


  /* Rotate old name_map files out of the way. */

  i=1, rot = NULL;
  do {
    if(rot != NULL) fclose(rot);
    snprintf(path, MAXPATHLEN, "%s/.diamond/name_map-%d", home, i);
    if((rot = fopen(path, "r")) != NULL)
      i++;
  }
  while(rot != NULL);
  
  for(; i>1; i--) {
    char oldpath[MAXPATHLEN], newpath[MAXPATHLEN];

    snprintf(oldpath, MAXPATHLEN, "%s/.diamond/name_map-%d", home, i-1);
    snprintf(newpath, MAXPATHLEN, "%s/.diamond/name_map-%d", home, i);

    if(rename(oldpath, newpath) < 0) {
      perror("rename");
      fprintf(stderr, "Failed executing rename from %s to %s\n", 
	      oldpath, newpath);
    }
  }
  snprintf(path, MAXPATHLEN, "%s/.diamond/name_map", home);
  if((rot = fopen(path, "r")) != NULL) {
    char oldpath[MAXPATHLEN], newpath[MAXPATHLEN];

    fclose(rot);

    snprintf(oldpath, MAXPATHLEN, "%s/.diamond/name_map", home);
    snprintf(newpath, MAXPATHLEN, "%s/.diamond/name_map-1", home);

    if(rename(oldpath, newpath) < 0) {
      perror("rename");
      fprintf(stderr, "Failed executing rename from %s to %s\n", 
	      oldpath, newpath);
    }
  }


  /* Rotate old gid_map files out of the way. */

  i=1, rot = NULL;
  do {
    if(rot != NULL) fclose(rot);
    snprintf(path, MAXPATHLEN, "%s/.diamond/gid_map-%d", home, i);
    rot = fopen(path, "r");
    if((rot = fopen(path, "r")) != NULL)
      i++;
  }
  while(rot != NULL);
  
  for(; i>1; i--) {
    char oldpath[MAXPATHLEN], newpath[MAXPATHLEN];

    snprintf(oldpath, MAXPATHLEN, "%s/.diamond/gid_map-%d", home, i-1);
    snprintf(newpath, MAXPATHLEN, "%s/.diamond/gid_map-%d", home, i);

    if(rename(oldpath, newpath) < 0) {
      perror("rename");
      fprintf(stderr, "Failed executing rename from %s to %s\n", 
	      oldpath, newpath);
    }
  }
  snprintf(path, MAXPATHLEN, "%s/.diamond/gid_map", home);
  if((rot = fopen(path, "r")) != NULL) {
    char oldpath[MAXPATHLEN], newpath[MAXPATHLEN];

    snprintf(oldpath, MAXPATHLEN, "%s/.diamond/gid_map", home);
    snprintf(newpath, MAXPATHLEN, "%s/.diamond/gid_map-1", home);

    if(rename(oldpath, newpath) < 0) {
      perror("rename");
      fprintf(stderr, "Failed executing rename from %s to %s\n", 
	      oldpath, newpath);
    }
  }


  /* Now that we have created space, parse the NEWSCOPE file and write
   * out new name_map and gid_map files for the Diamond application to read. */

  snprintf(path, MAXPATHLEN, "%s/.diamond/NEWSCOPE", home);
  if((fp = fopen(path, "r")) == NULL) {
    fprintf(stderr, "Couldn't open scope file %s!\n", path);
    goto exit_failure;
  }

  snprintf(path, MAXPATHLEN, "%s/.diamond/name_map", home);
  if((np = fopen(path, "w")) == NULL) {
    fprintf(stderr, "Couldn't open %s!\n", path);
    goto exit_failure;
  }

  snprintf(path, MAXPATHLEN, "%s/.diamond/gid_map", home);
  if((gp = fopen(path, "w")) == NULL) {
    fprintf(stderr, "Couldn't open %s!\n", path);
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
