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

#ifndef OPENDIAMOND_SRC_FILTER_RUNNER_UTIL_H_
#define OPENDIAMOND_SRC_FILTER_RUNNER_UTIL_H_

#include <glib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

struct ohandle {
  GHashTable *attributes;
};

struct attribute {
  size_t len;
  void *data;
};

void attribute_destroy(gpointer user_data);

int get_size(FILE *in);

char *get_string(FILE *in);

char **get_strings(FILE *in);

void *get_binary(FILE *in, int *len_OUT);

bool get_boolean(FILE *in);

void get_blank(FILE *in);

double get_double(FILE *in);

void send_tag(FILE *out, const char *tag);

void send_int(FILE *out, int i);

void send_string(FILE *out, const char *str);

void send_binary(FILE *out, int len, const void *data);

void send_blank(FILE *out);

void send_double(FILE *out, double d);

#endif
