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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "helper.h"

int 
is_file(char *path) {
  struct stat buf;

  if(path == NULL) {
    fprintf(stderr, "NULL file string!\n");
    return -1;
  }
  
  if(stat(path, &buf)) {
    perror("stat");
    return -1;
  }
  
  if(S_ISREG(buf.st_mode))
    return 1;
  
  return 0;   
}


int 
is_dir(char *path) {
  struct stat buf;

  if(path == NULL) {
    fprintf(stderr, "NULL file string!\n");
    return -1;
  }
  
  if(stat(path, &buf)) {
    perror("stat");
    return -1;
  }
  
  if(S_ISDIR(buf.st_mode))
    return 1;
  
  return 0;   
}


int
binary_to_hex_string(int len, char *dest, int dest_len, 
		     unsigned char *data, int data_len) {
  int i;

  if((dest == NULL) || (data == NULL))
    return -1;

  if((dest_len <= 0) || (data_len <= 0))
    return -1;

  if(dest_len < len)
    return -1;

  for(i = 0; i < len/2; i++) {
    char high, low;
    
    low = (data[i] & 0xF);
    if(low < 10)
      dest[2*i] = low + '0';
    else
      dest[2*i] = low - 10 + 'A';

    high = ((data[i]>>4) & 0xF);
    if(high < 10)
      dest[2*i+1] = high + '0';
    else
      dest[2*i+1] = high - 10 + 'A';
  }

  dest[2*i] = '\0';

  return 0;
}


char *
strip_username(char *src) {
  char *travp;

  for(travp = src; (travp[0] != '@') && (travp < (src + strlen(src))); travp++)
    continue;

  travp++;
  if(travp >= (src + strlen(src)))
    return NULL;

  return travp;
}


/* 
 * Looks for the first '.' in the basename of the filename.
 */
char *
parse_extension(char *src) {
  char *travp;

  for(travp = src; (travp[0] != '.') && (travp < (src + strlen(src))); travp++)
    continue;

  if(travp >= (src + strlen(src)))
    return NULL;

  return travp;
}


/*
 * Inserts colons between each byte (2 chars) in a hex string.
 */

#define GIDCHARS 16

int
insert_gid_colons(char *dest, char *src) {
  int i, idx;

  if((src == NULL) || (dest == NULL))
    return -1;
  
  dest[0] = src[0];
  dest[1] = src[1];
  for(i=2, idx=2; i<GIDCHARS; i+=2, idx+=3) {
    dest[idx] = ':';
    dest[idx+1] = src[i];
    dest[idx+2] = src[i+1];
  }

  dest[idx] = '\0';

  return 0;
}
