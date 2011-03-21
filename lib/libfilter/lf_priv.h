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

#ifndef OPENDIAMOND_LIB_LIBFILTER_LF_PRIV_H_
#define OPENDIAMOND_LIB_LIBFILTER_LF_PRIV_H_

#include <stdio.h>
#include <stdbool.h>
#include "lib_filter.h"

extern struct lf_state {
  const char *filter_name;
  FILE *in;
  FILE *out;
} lf_state;

void lf_init(void);
void lf_run_filter(char *filter_name, filter_init_proto init,
                   filter_eval_proto eval, char **args, void *blob,
                   unsigned bloblen);

lf_obj_handle_t lf_obj_handle_new(void);
void lf_obj_handle_free(lf_obj_handle_t obj);

void lf_start_output(void);
void lf_end_output(void);

#endif
