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

#include "lib_filter.h"
#include "filter-runner.h"
#include "util.h"

const char *_filter_name;
FILE *_in;
FILE *_out;

struct filter_ops {
  filter_eval_proto eval;
  filter_fini_proto fini;
  void *data;
};

static void assert_result(int result) {
  if (result == -1) {
    perror("error");
    exit(EXIT_FAILURE);
  }
}

static void init_file_descriptors(int *stdin_orig, int *stdout_orig,
				  int *stdout_log) {
  int stdout_pipe[2];

  // save orig stdin/stdout
  *stdin_orig = dup(0);
  assert_result(*stdin_orig);
  *stdout_orig = dup(1);
  assert_result(*stdout_orig);

  // make pipes
  assert_result(pipe(stdout_pipe));

  // open /dev/null to stdin
  int devnull = open("/dev/null", O_RDONLY);
  assert_result(devnull);
  assert_result(dup2(devnull, 0));
  assert_result(close(devnull));

  // dup to stdout
  assert_result(dup2(stdout_pipe[1], 1));
  assert_result(close(stdout_pipe[1]));

  // save
  *stdout_log = stdout_pipe[0];
}

static void init_filter(FILE *in, struct filter_ops *ops) {
  char *error;

  // read shared object name
  char *filename = get_string(in);
  fprintf(stderr, "filename: %s\n", filename);

  // read init function name
  char *init_name = get_string(in);
  fprintf(stderr, "init_name: %s\n", init_name);

  // read eval function name
  char *eval_name = get_string(in);
  fprintf(stderr, "eval_name: %s\n", eval_name);

  // read fini function name
  char *fini_name = get_string(in);
  fprintf(stderr, "fini_name: %s\n", fini_name);

  // read argument list
  char **args = get_strings(in);
  fprintf(stderr, "args len: %d\n", g_strv_length(args));

  // read name
  _filter_name = get_string(in);
  fprintf(stderr, "filter_name: %s\n", _filter_name);

  // read blob
  int bloblen;
  uint8_t *blob = get_binary(in, &bloblen);
  fprintf(stderr, "bloblen: %d\n", bloblen);

  // dlopen
  void *handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  // init
  int (*filter_init)(int num_arg, char **args, int bloblen,
		     void *blob_data, const char * filt_name,
		     void **filter_args);
  *(void **) (&filter_init) = dlsym(handle, init_name);
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error);
    exit(EXIT_FAILURE);
  }

  int result = (*filter_init)(g_strv_length(args), args,
			      bloblen, blob,
			      _filter_name, &ops->data);
  if (result != 0) {
    fprintf(stderr, "filter init failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "filter init success\n");


  // save
  *(void **) (&ops->eval) = dlsym(handle, eval_name);
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error);
    exit(EXIT_FAILURE);
  }

  *(void **) (&ops->fini) = dlsym(handle, fini_name);
  if ((error = dlerror()) != NULL) {
    fprintf(stderr, "%s\n", error);
    exit(EXIT_FAILURE);
  }

  // free
  g_free(filename);
  g_free(init_name);
  g_free(eval_name);
  g_free(fini_name);
  g_strfreev(args);
  g_free(blob);
}


static void run_filter(struct filter_ops *ops,
		       int stdout_log) {
  // eval loop
  while (true) {
    GHashTable *attrs = g_hash_table_new_full(g_str_hash, g_str_equal,
					      g_free, attribute_destroy);

    // init ohandle
    struct ohandle ohandle = { attrs };

    // eval and return result
    int result = (*ops->eval)(&ohandle, ops->data);
    send_result(_out, result);

    g_hash_table_unref(attrs);
  }
}



int main(void) {
  struct filter_ops ops;

  int stdin_orig;
  int stdout_orig;

  int stdout_log;

  if (!g_thread_supported ()) g_thread_init (NULL);

  init_file_descriptors(&stdin_orig, &stdout_orig,
			&stdout_log);

  // make files
  _in = fdopen(stdin_orig, "r");
  if (!_in) {
    perror("Can't open stdin_orig");
    exit(EXIT_FAILURE);
  }
  _out = fdopen(stdout_orig, "w");
  if (!_out) {
    perror("Can't open stdout_orig");
    exit(EXIT_FAILURE);
  }

  // start logging thread
  struct logger_data data = { _out, stdout_log };
  if (g_thread_create(logger, &data, false, NULL) == NULL) {
    fprintf(stderr, "Can't create logger thread\n");
    exit(EXIT_FAILURE);
  }

  init_filter(_in, &ops);
  run_filter(&ops,
	     stdout_log);

  return EXIT_SUCCESS;
}
