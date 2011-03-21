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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "lib_filter.h"
#include "lf_protocol.h"
#include "lf_priv.h"

struct filter_ops {
  filter_init_proto init;
  filter_eval_proto eval;
  char *filter_name;
  char **args;
  void *blob;
  int bloblen;
};

static void handshake(FILE *in, FILE *out, struct filter_ops *ops) {
  char *error;

  // read shared object name
  char *filename = lf_get_string(in);
  //  g_message("filename: %s", filename);

  // read init function name
  char *init_name = lf_get_string(in);
  //  g_message("init_name: %s", init_name);

  // read eval function name
  char *eval_name = lf_get_string(in);
  //  g_message("eval_name: %s", eval_name);

  // read fini function name
  char *fini_name = lf_get_string(in);
  //  g_message("fini_name: %s", fini_name);

  // read argument list
  ops->args = lf_get_strings(in);
  /*
  g_message("args len: %d", g_strv_length(ops->args));
  for (char **arg = ops->args; *arg != NULL; arg++) {
    g_message(" arg: %s", *arg);
  }
  */

  // read blob
  ops->blob = lf_get_binary(in, &ops->bloblen);
  //  g_message("bloblen: %d", ops->bloblen);

  // read name
  ops->filter_name = lf_get_string(in);
  //  g_message("filter_name: %s", _filter_name);

  // dlopen
  void *handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
  if (!handle) {
    g_warning("%s", dlerror());
    exit(EXIT_FAILURE);
  }

  // find functions
  ops->init = dlsym(handle, init_name);
  if ((error = dlerror()) != NULL) {
    //    g_warning("%s", error);
    exit(EXIT_FAILURE);
  }

  ops->eval = dlsym(handle, eval_name);
  if ((error = dlerror()) != NULL) {
    //    g_warning("%s", error);
    exit(EXIT_FAILURE);
  }

  // The fini function is unused, but we still check for it
  dlsym(handle, fini_name);
  if ((error = dlerror()) != NULL) {
    //    g_warning("%s", error);
    exit(EXIT_FAILURE);
  }

  // report load success
  lf_start_output();
  lf_send_tag(out, "functions-resolved");
  lf_end_output();

  // free
  g_free(filename);
  g_free(init_name);
  g_free(eval_name);
  g_free(fini_name);
}


int main(void) {
  struct filter_ops ops;

  lf_init();
  handshake(lf_state.in, lf_state.out, &ops);
  lf_run_filter(ops.filter_name, ops.init, ops.eval, ops.args, ops.blob,
                ops.bloblen);

  return EXIT_SUCCESS;
}
