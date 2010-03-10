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

static GStaticMutex out_mutex = G_STATIC_MUTEX_INIT;

gpointer logger(gpointer data) {
  struct logger_data *l = data;

  FILE *out = l->out;
  int stdout_log = l->stdout_log;

  // block signals
  sigset_t set;
  sigfillset(&set);
  pthread_sigmask(SIG_SETMASK, &set, NULL);

  fprintf(stderr, "Logger thread is ready\n");

  // go
  while (true) {
    ssize_t size;
    uint8_t buf[BUFSIZ];

    // read from fd
    size = read(stdout_log, buf, BUFSIZ);
    if (size <= 0) {
      perror("Can't read");
      exit(EXIT_FAILURE);
    }

    // print it
    g_static_mutex_lock(&out_mutex);
    if (fprintf(out, "stdout\n%d\n", size) == -1) {
      if (feof(out)) {
	fprintf(stderr, "EOF\n");
      } else {
	perror("Can't write");
      }
      exit(EXIT_FAILURE);
    }
    if (fwrite(buf, size, 1, out) != 1) {
      if (feof(out)) {
	fprintf(stderr, "EOF\n");
      } else {
	perror("Can't write");
      }
      exit(EXIT_FAILURE);
    }
    if (fprintf(out, "\n") == -1) {
      if (feof(out)) {
	fprintf(stderr, "EOF\n");
      } else {
	perror("Can't write");
      }
      exit(EXIT_FAILURE);
    }
    g_static_mutex_unlock(&out_mutex);
  }

  return NULL;
}

void attribute_destroy(gpointer user_data) {
  struct attribute *attr = user_data;

  g_free(attr->data);
  g_slice_free(struct attribute, attr);
}

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
    if (feof(in)) {
      fprintf(stderr, "EOF\n");
    } else {
      perror("Can't read string");
    }
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

void *get_binary(FILE *in, int *len_OUT) {
  int size = get_size(in);
  *len_OUT = size;

  uint8_t *binary = NULL;

  if (size > 0) {
    binary = g_malloc(size);

    if (fread(binary, size, 1, in) != 1) {
      if (feof(in)) {
	fprintf(stderr, "EOF\n");
      } else {
	perror("Can't read binary");
      }
      exit(EXIT_FAILURE);
    }
  }

  if (size != -1) {
    // read trailing '\n'
    getc(in);
  }

  return binary;
}

void send_binary(FILE *out, int len, void *data) {
  g_static_mutex_lock(&out_mutex);
  if (fprintf(out, "%d\n", len) == -1) {
    if (feof(out)) {
      fprintf(stderr, "EOF\n");
    } else {
      perror("Can't write binary length");
    }
    exit(EXIT_FAILURE);
  }
  if (fwrite(data, len, 1, out) != 1) {
    if (feof(out)) {
      fprintf(stderr, "EOF\n");
    } else {
      perror("Can't write binary");
    }
    exit(EXIT_FAILURE);
  }
  if (fprintf(out, "\n") == -1) {
    if (feof(out)) {
      fprintf(stderr, "EOF\n");
    } else {
      perror("Can't write end of binary");
    }
    exit(EXIT_FAILURE);
  }
  g_static_mutex_unlock(&out_mutex);
}


void send_tag(FILE *out, const char *tag) {
  g_static_mutex_lock(&out_mutex);
  if (fprintf(out, "%s\n", tag) == -1) {
    if (feof(out)) {
      fprintf(stderr, "EOF\n");
    } else {
      perror("Can't write tag");
    }
    exit(EXIT_FAILURE);
  }
  g_static_mutex_unlock(&out_mutex);
}

void send_int(FILE *out, int i) {
  char *str = g_strdup_printf("%d", i);
  send_string(out, str);
  g_free(str);
}

void send_string(FILE *out, const char *str) {
  int len = strlen(str);
  g_static_mutex_lock(&out_mutex);
  if (fprintf(out, "%d\n%s\n", len, str) == -1) {
    if (feof(out)) {
      fprintf(stderr, "EOF\n");
    } else {
      perror("Can't write string");
    }
    exit(EXIT_FAILURE);
  }
  g_static_mutex_unlock(&out_mutex);
}


struct attribute *get_attribute(FILE *in, FILE *out,
				struct ohandle *ohandle, const char *name) {
  // look up in hash table
  struct attribute *attr = g_hash_table_lookup(ohandle->attributes,
					       name);

  // retrieve?
  if (attr == NULL) {
    send_tag(out, "get-attribute");
    send_string(out, name);

    int len;
    void *data = get_binary(in, &len);

    if (len == -1) {
      // no attribute
      return NULL;
    }

    attr = g_slice_new(struct attribute);
    attr->data = data;
    attr->len = len;

    g_hash_table_insert(ohandle->attributes, g_strdup(name), attr);
  }

  return attr;
}

bool get_boolean(FILE *in) {
  char *str = get_string(in);
  bool result = (strcmp(str, "true") == 0);
  g_free(str);

  return result;
}

void send_result(FILE *out, int result) {
  send_tag(out, "result");
  send_int(out, result);
}
