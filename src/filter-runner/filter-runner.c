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
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>

#include "lib_filter.h"

static GStaticMutex out_mutex = G_STATIC_MUTEX_INIT;

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

static int get_size_or_die(FILE *in, FILE *err) {
  char *line = NULL;
  size_t n;
  int result;

  if (getline(&line, &n, in) == -1) {
    fprintf(err, "Can't read size\n");
    exit(EXIT_FAILURE);
  }

  // if there is no string, then return -1
  if (strlen(line) == 1) {
    result = -1;
  } else {
    result = atoi(line);
  }

  free(line);

  fprintf(err, "size: %d\n", result);
  return result;
}

static char *get_string_or_die(FILE *in, FILE *err) {
  int size = get_size_or_die(in, err);

  if (size == -1) {
    return NULL;
  }

  char *result = g_malloc(size + 1);
  result[size] = '\0';

  if (fread(result, size, 1, in) != 1) {
    fprintf(err, "Can't read string\n");
    exit(EXIT_FAILURE);
  }

  // read trailing '\n'
  getc(in);

  return result;
}

static char **get_strings_or_die(FILE *in, FILE *err) {
  GSList *list = NULL;

  char *str;
  while ((str = get_string_or_die(in, err)) != NULL) {
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

static void *get_blob_or_die(FILE *in, FILE *err, int *bloblen_OUT) {
  int size = get_size_or_die(in, err);
  *bloblen_OUT = size;

  uint8_t *blob;

  if (size == 0) {
    blob = NULL;
  } else {
    blob = g_malloc(size);

    if (fread(blob, size, 1, in) != 1) {
      fprintf(err, "Can't read blob\n");
      exit(EXIT_FAILURE);
    }
  }

  // read trailing '\n'
  getc(in);

  return blob;
}

static void init_file_descriptors(int *stdin_orig, int *stdout_orig, int *stderr_orig,
				  int *stdout_log, int *stderr_log) {
  int stdout_pipe[2];
  int stderr_pipe[2];

  // save orig stdin/stdout/stderr
  *stdin_orig = dup(0);
  assert_result(*stdin_orig);
  *stdout_orig = dup(1);
  assert_result(*stdout_orig);
  *stderr_orig = dup(2);
  assert_result(*stderr_orig);

  // make pipes
  assert_result(pipe(stdout_pipe));
  assert_result(pipe(stderr_pipe));

  // open /dev/null to stdin
  int devnull = open("/dev/null", O_RDONLY);
  assert_result(devnull);
  assert_result(dup2(devnull, 0));
  assert_result(close(devnull));

  // dup to stdout/stderr
  assert_result(dup2(stdout_pipe[1], 1));
  assert_result(dup2(stderr_pipe[1], 2));

  // save
  *stdout_log = stdout_pipe[0];
  *stderr_log = stderr_pipe[0];
}

static void init_filter(FILE *in, FILE *err, struct filter_ops *ops) {
  char *error;

  // read shared object name
  char *filename = get_string_or_die(in, err);
  fprintf(err, "filename: %s\n", filename);

  // read init function name
  char *init_name = get_string_or_die(in, err);
  fprintf(err, "init_name: %s\n", init_name);

  // read eval function name
  char *eval_name = get_string_or_die(in, err);
  fprintf(err, "eval_name: %s\n", eval_name);

  // read fini function name
  char *fini_name = get_string_or_die(in, err);
  fprintf(err, "fini_name: %s\n", fini_name);

  // read argument list
  char **args = get_strings_or_die(in, err);
  fprintf(err, "args len: %d\n", g_strv_length(args));

  // read name
  char *filter_name = get_string_or_die(in, err);
  fprintf(err, "filter_name: %s\n", filter_name);

  // read blob
  int bloblen;
  uint8_t *blob = get_blob_or_die(in, err, &bloblen);
  fprintf(err, "bloblen: %d\n", bloblen);

  // dlopen
  void *handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
  if (!handle) {
    fprintf(err, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  // init
  int (*filter_init)(int num_arg, char **args, int bloblen,
		     void *blob_data, const char * filt_name,
		     void **filter_args);
  *(void **) (&filter_init) = dlsym(handle, init_name);
  if ((error = dlerror()) != NULL) {
    fprintf(err, "%s\n", error);
    exit(EXIT_FAILURE);
  }

  int result = (*filter_init)(g_strv_length(args), args,
			      bloblen, blob,
			      filter_name, &ops->data);
  if (result != 0) {
    fprintf(err, "filter init failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(err, "filter init success\n");

  // save
  *(void **) (&ops->eval) = dlsym(handle, eval_name);
  if ((error = dlerror()) != NULL) {
    fprintf(err, "%s\n", error);
    exit(EXIT_FAILURE);
  }

  *(void **) (&ops->fini) = dlsym(handle, fini_name);
  if ((error = dlerror()) != NULL) {
    fprintf(err, "%s\n", error);
    exit(EXIT_FAILURE);
  }

  // free
  g_free(filename);
  g_free(init_name);
  g_free(eval_name);
  g_free(fini_name);
  g_strfreev(args);
  g_free(filter_name);
  g_free(blob);
}


struct logger_data {
  FILE *out;
  FILE *err;
  int stdout_log;
  int stderr_log;
};


static void write_log_message(const char *tag, int fd, FILE *out, FILE *err) {
  ssize_t size;
  uint8_t buf[BUFSIZ];

  // read from fd
  size = read(fd, buf, BUFSIZ);
  if (size <= 0) {
    fprintf(err, "Can't read\n");
    exit(EXIT_FAILURE);
  }

  // print it
  g_static_mutex_lock(&out_mutex);
  if (fprintf(out, "%s\n%d\n", tag, size) == -1) {
    fprintf(err, "Can't write\n");
    exit(EXIT_FAILURE);
  }
  if (fwrite(buf, size, 1, out) != 1) {
    fprintf(err, "Can't write\n");
    exit(EXIT_FAILURE);
  }
  if (fprintf(out, "\n") == -1) {
    fprintf(err, "Can't write\n");
    exit(EXIT_FAILURE);
  }
  g_static_mutex_unlock(&out_mutex);
}

static gpointer logger(gpointer data) {
  struct logger_data *l = data;

  FILE *out = l->out;
  FILE *err = l->err;
  int stdout_log = l->stdout_log;
  int stderr_log = l->stderr_log;

  // block signals
  sigset_t set;
  sigfillset(&set);
  pthread_sigmask(SIG_SETMASK, &set, NULL);

  // go
  while (true) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(stdout_log, &rfds);
    FD_SET(stderr_log, &rfds);

    int val = select(MAX(stdout_log, stderr_log) + 1, &rfds, NULL, NULL, NULL);

    if (val == -1 && errno == EINTR) {
      continue;
    } else if (val == -1) {
      exit(EXIT_FAILURE);
    }

    if (FD_ISSET(stdout_log, &rfds)) {
      write_log_message("log-stdout", stdout_log, out, err);
    }
    if (FD_ISSET(stderr_log, &rfds)) {
      write_log_message("log-stderr", stderr_log, out, err);
    }
  }

  return NULL;
}

static void run_filter(struct filter_ops *ops,
		       int stdin_orig, int stdout_orig, int stderr_orig,
		       int stdout_log, int stderr_log) {
  // make files
  FILE *err = fdopen(stderr_orig, "r");
  if (!err) {
    exit(EXIT_FAILURE);
  }

  FILE *in = fdopen(stdin_orig, "r");
  if (!in) {
    fprintf(err, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  FILE *out = fdopen(stdout_orig, "r");
  if (!out) {
    fprintf(err, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // start logging thread
  struct logger_data data = { out, err, stdout_log, stderr_log };
  if (g_thread_create(logger, &data, false, NULL) == NULL) {
    fprintf(err, "Can't create logger thread\n");
    exit(EXIT_FAILURE);
  }

  while (true) {
    
  }
}



int main(void) {
  struct filter_ops ops;

  int stdin_orig;
  int stdout_orig;
  int stderr_orig;

  int stdout_log;
  int stderr_log;

  if (!g_thread_supported ()) g_thread_init (NULL);

  init_filter(stdin, stderr, &ops);
  init_file_descriptors(&stdin_orig, &stdout_orig, &stderr_orig,
			&stdout_log, &stderr_log);
  run_filter(&ops,
	     stdin_orig, stdout_orig, stderr_orig,
	     stdout_log, stderr_log);

  return EXIT_SUCCESS;
}
