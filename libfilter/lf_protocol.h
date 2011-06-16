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

#ifndef OPENDIAMOND_LIB_LIBFILTER_LF_PROTOCOL_H_
#define OPENDIAMOND_LIB_LIBFILTER_LF_PROTOCOL_H_

#include <stdbool.h>
#include "lib_filter.h"

int lf_get_size(FILE *in);

char *lf_get_string(FILE *in);

char **lf_get_strings(FILE *in);

void *lf_get_binary(FILE *in, int *len_OUT);

bool lf_get_boolean(FILE *in);

void lf_get_blank(FILE *in);

double lf_get_double(FILE *in);

void lf_send_tag(FILE *out, const char *tag);

void lf_send_int(FILE *out, int i);

void lf_send_string(FILE *out, const char *str);

void lf_send_binary(FILE *out, int len, const void *data);

void lf_send_blank(FILE *out);

void lf_send_double(FILE *out, double d);

#endif
