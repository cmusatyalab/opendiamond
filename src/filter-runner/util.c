/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 5
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2010 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "util.h"


int get_size(FILE *in) {
  char *line = NULL;
  size_t n;
  int result;

  if (getline(&line, &n, in) == -1) {
    fprintf(stderr, "Can't read size\n");
    exit(EXIT_FAILURE);
  }

  // if there is no string, then return -1
  if (strlen(line) == 1) {
    result = -1;
  } else {
    result = atoi(line);
  }

  free(line);

  fprintf(stderr, "size: %d\n", result);
  return result;
}

char *get_string(FILE *in) {
  int size = get_size(in);

  if (size == -1) {
    return NULL;
  }

  char *result = g_malloc(size + 1);
  result[size] = '\0';

  if (fread(result, size, 1, in) != 1) {
    fprintf(stderr, "Can't read string\n");
    exit(EXIT_FAILURE);
  }

  // read trailing '\n'
  getc(in);

  return result;
}

char **get_strings(FILE *in) {
  GSList *list = NULL;

  char *str;
  while ((str = get_string(in)) != NULL) {
    list = g_slist_prepend(list, str);
  }

  // convert to strv
  int len = g_slist_length(list);
  char **result = g_new(char *, len + 1);
  result[len] = NULL;

  list = g_slist_reverse(list);

  int i = 0;
  while (list != NULL) {
    result[i++] = list->data;
    list = g_slist_delete_link(list, list);
  }

  return result;
}

void *get_blob(FILE *in, int *bloblen_OUT) {
  int size = get_size(in);
  *bloblen_OUT = size;

  uint8_t *blob;

  if (size == 0) {
    blob = NULL;
  } else {
    blob = g_malloc(size);

    if (fread(blob, size, 1, in) != 1) {
      fprintf(stderr, "Can't read blob\n");
      exit(EXIT_FAILURE);
    }
  }

  // read trailing '\n'
  getc(in);

  return blob;
}


void send_tag(FILE *out, const char *tag) {
  fprintf(out, "%s\n", tag);
}

void send_int(FILE *out, int i) {
  char *str = g_strdup_printf("%d", i);
  send_string(out, str);
  g_free(str);
}

void send_string(FILE *out, const char *str) {
  int len = strlen(str);
  fprintf(out, "%d\n%s\n", len, str);
}
