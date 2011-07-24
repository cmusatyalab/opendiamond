/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 6
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

struct lf_state lf_state;

static GStaticMutex out_mutex = G_STATIC_MUTEX_INIT;

void lf_start_output(void) {
  g_static_mutex_lock(&out_mutex);
}

void lf_end_output(void) {
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


static gpointer logger(gpointer data) {
  int stdout_log = GPOINTER_TO_INT(data);

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
    lf_start_output();
    lf_send_tag(lf_state.out, "stdout");
    lf_send_binary(lf_state.out, size, buf);
    lf_end_output();
  }

  return NULL;
}

static void lf_init(void) {
  int stdin_orig;
  int stdout_orig;
  int stdout_log;

  if (!g_thread_supported ()) g_thread_init (NULL);

  init_file_descriptors(&stdin_orig, &stdout_orig,
			&stdout_log);

  // make files
  lf_state.in = fdopen(stdin_orig, "r");
  if (!lf_state.in) {
    perror("Can't open stdin_orig");
    exit(EXIT_FAILURE);
  }
  lf_state.out = fdopen(stdout_orig, "w");
  if (!lf_state.out) {
    perror("Can't open stdout_orig");
    exit(EXIT_FAILURE);
  }

  // unbuffer fake stdout
  setbuf(stdout, NULL);

  // start logging thread
  if (g_thread_create(logger, GINT_TO_POINTER(stdout_log), false,
                      NULL) == NULL) {
    g_warning("Can't create logger thread");
    exit(EXIT_FAILURE);
  }
}

static void lf_run_filter(char *filter_name, filter_init_proto init,
                          filter_eval_proto eval_int,
                          filter_eval_double_proto eval_double,
                          char **args, void *blob, unsigned bloblen) {
  // record the filter name
  lf_state.filter_name = filter_name;

  // initialize the filter
  void *data;
  int result = init(g_strv_length(args), (const char * const *) args,
                    bloblen, blob, filter_name, &data);
  if (result != 0) {
    g_warning("filter init failed");
    exit(EXIT_FAILURE);
  }

  // report init success
  lf_start_output();
  lf_send_tag(lf_state.out, "init-success");
  lf_end_output();

  // eval loop
  while (true) {
    // init ohandle
    lf_obj_handle_t obj = lf_obj_handle_new();

    // eval and return result
    double result;
    if (eval_double) {
      result = eval_double(obj, data);
    } else {
      result = eval_int(obj, data);
    }
    lf_start_output();
    lf_send_tag(lf_state.out, "result");
    lf_send_double(lf_state.out, result);
    lf_end_output();

    lf_obj_handle_free(obj);
  }
}

static void _lf_main(filter_init_proto init, filter_eval_proto eval,
                     filter_eval_double_proto eval_double) {
  // set up file descriptors
  lf_init();

  // read protocol version
  double version = lf_get_double(lf_state.in);
  if (version != 1) {
    g_error("Unknown protocol version %d", (int) version);
    exit(EXIT_FAILURE);
  }

  // read name
  char *filter_name = lf_get_string(lf_state.in);

  // read argument list
  char **args = lf_get_strings(lf_state.in);

  // read blob
  int bloblen;
  void *blob = lf_get_binary(lf_state.in, &bloblen);

  // run the filter loop
  lf_run_filter(filter_name, init, eval, eval_double, args, blob, bloblen);
}

void lf_main(filter_init_proto init, filter_eval_proto eval) {
  _lf_main(init, eval, NULL);
}

void lf_main_double(filter_init_proto init, filter_eval_double_proto eval) {
  _lf_main(init, NULL, eval);
}
