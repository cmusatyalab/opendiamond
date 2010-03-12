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
#include "filter-runner-util.h"

const char *_filter_name;
FILE *_in;
FILE *_out;

struct filter_ops {
  filter_eval_proto eval;
  filter_fini_proto fini;
  void *data;
};

static GStaticMutex out_mutex = G_STATIC_MUTEX_INIT;

void start_output(void) {
  g_static_mutex_lock(&out_mutex);
}

void end_output(void) {
  g_static_mutex_unlock(&out_mutex);
}

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

static void init_filter(FILE *in, FILE *out, struct filter_ops *ops) {
  char *error;

  // read shared object name
  char *filename = get_string(in);
  //  g_message("filename: %s", filename);

  // read init function name
  char *init_name = get_string(in);
  //  g_message("init_name: %s", init_name);

  // read eval function name
  char *eval_name = get_string(in);
  //  g_message("eval_name: %s", eval_name);

  // read fini function name
  char *fini_name = get_string(in);
  //  g_message("fini_name: %s", fini_name);

  // read argument list
  char **args = get_strings(in);
  //  g_message("args len: %d", g_strv_length(args));

  // read blob
  int bloblen;
  uint8_t *blob = get_binary(in, &bloblen);
  //  g_message("bloblen: %d", bloblen);

  // read name
  _filter_name = get_string(in);
  //  g_message("filter_name: %s", _filter_name);

  // dlopen
  void *handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
  if (!handle) {
    g_warning("%s", dlerror());
    exit(EXIT_FAILURE);
  }

  // find functions
  int (*filter_init)(int num_arg, char **args, int bloblen,
		     void *blob_data, const char * filt_name,
		     void **filter_args);
  *(void **) (&filter_init) = dlsym(handle, init_name);
  if ((error = dlerror()) != NULL) {
    //    g_warning("%s", error);
    exit(EXIT_FAILURE);
  }

  *(void **) (&ops->eval) = dlsym(handle, eval_name);
  if ((error = dlerror()) != NULL) {
    //    g_warning("%s", error);
    exit(EXIT_FAILURE);
  }

  *(void **) (&ops->fini) = dlsym(handle, fini_name);
  if ((error = dlerror()) != NULL) {
    //    g_warning("%s", error);
    exit(EXIT_FAILURE);
  }

  // report load success
  start_output();
  send_tag(out, "functions-resolved");
  end_output();


  // now it is safe to write to stdout
  //  g_debug("filter_name: %s", _filter_name);

  // init
  int result = (*filter_init)(g_strv_length(args), args,
			      bloblen, blob,
			      _filter_name, &ops->data);
  if (result != 0) {
    g_warning("filter init failed");
    exit(EXIT_FAILURE);
  }
  //  g_message("filter init success");

  // free
  g_free(filename);
  g_free(init_name);
  g_free(eval_name);
  g_free(fini_name);
  g_strfreev(args);
  g_free(blob);
}


struct logger_data {
  FILE *out;
  int stdout_log;
};


static gpointer logger(gpointer data) {
  struct logger_data *l = data;

  FILE *out = l->out;
  int stdout_log = l->stdout_log;

  // block signals
  sigset_t set;
  sigfillset(&set);
  pthread_sigmask(SIG_SETMASK, &set, NULL);

  //  g_message("Logger thread is ready");

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
    start_output();
    send_tag(out, "stdout");
    send_binary(out, size, buf);
    end_output();
  }

  return NULL;
}

static void send_result(FILE *out, int result) {
  start_output();
  send_tag(out, "result");
  send_int(out, result);
  end_output();
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

  // unbuffer fake stdout
  setbuf(stdout, NULL);

  // start logging thread
  struct logger_data data = { _out, stdout_log };
  if (g_thread_create(logger, &data, false, NULL) == NULL) {
    g_warning("Can't create logger thread");
    exit(EXIT_FAILURE);
  }

  init_filter(_in, _out, &ops);
  run_filter(&ops,
	     stdout_log);

  return EXIT_SUCCESS;
}
