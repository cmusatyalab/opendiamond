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

struct ohandle {
  FILE *in;
  FILE *out;
};

static void assert_result(int result) {
  if (result == -1) {
    perror("error");
    exit(EXIT_FAILURE);
  }
}

static int get_size_or_die(FILE *in) {
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

static char *get_string_or_die(FILE *in) {
  int size = get_size_or_die(in);

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

static char **get_strings_or_die(FILE *in) {
  GSList *list = NULL;

  char *str;
  while ((str = get_string_or_die(in)) != NULL) {
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

static void *get_blob_or_die(FILE *in, int *bloblen_OUT) {
  int size = get_size_or_die(in);
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
  char *filename = get_string_or_die(in);
  fprintf(stderr, "filename: %s\n", filename);

  // read init function name
  char *init_name = get_string_or_die(in);
  fprintf(stderr, "init_name: %s\n", init_name);

  // read eval function name
  char *eval_name = get_string_or_die(in);
  fprintf(stderr, "eval_name: %s\n", eval_name);

  // read fini function name
  char *fini_name = get_string_or_die(in);
  fprintf(stderr, "fini_name: %s\n", fini_name);

  // read argument list
  char **args = get_strings_or_die(in);
  fprintf(stderr, "args len: %d\n", g_strv_length(args));

  // read name
  char *filter_name = get_string_or_die(in);
  fprintf(stderr, "filter_name: %s\n", filter_name);

  // read blob
  int bloblen;
  uint8_t *blob = get_blob_or_die(in, &bloblen);
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
			      filter_name, &ops->data);
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
  g_free(filter_name);
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

  fprintf(stderr, "Logger thread is ready\n");

  // go
  while (true) {
    ssize_t size;
    uint8_t buf[BUFSIZ];

    // read from fd
    size = read(stdout_log, buf, BUFSIZ);
    if (size <= 0) {
      fprintf(stderr, "Can't read\n");
      exit(EXIT_FAILURE);
    }

    // print it
    g_static_mutex_lock(&out_mutex);
    if (fprintf(out, "stdout\n%d\n", size) == -1) {
      perror("Can't write");
      exit(EXIT_FAILURE);
    }
    if (fwrite(buf, size, 1, out) != 1) {
      perror("Can't write");
      exit(EXIT_FAILURE);
    }
    if (fprintf(out, "\n") == -1) {
      perror("Can't write");
      exit(EXIT_FAILURE);
    }
    g_static_mutex_unlock(&out_mutex);
  }

  return NULL;
}


static void send_result(FILE *out, int result) {
  
}


static void run_filter(struct filter_ops *ops,
		       int stdin_orig, int stdout_orig,
		       int stdout_log) {
  // make files
  FILE *in = fdopen(stdin_orig, "r");
  if (!in) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  FILE *out = fdopen(stdout_orig, "w");
  if (!out) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // start logging thread
  struct logger_data data = { out, stdout_log };
  if (g_thread_create(logger, &data, false, NULL) == NULL) {
    fprintf(stderr, "Can't create logger thread\n");
    exit(EXIT_FAILURE);
  }

  struct ohandle ohandle = { in, out };

  while (true) {
    // eval and return result
    int result = (*ops->eval)(&ohandle, ops->data);

    send_result(out, result);
  }
}



int main(void) {
  struct filter_ops ops;

  int stdin_orig;
  int stdout_orig;

  int stdout_log;

  if (!g_thread_supported ()) g_thread_init (NULL);

  init_filter(stdin, &ops);
  init_file_descriptors(&stdin_orig, &stdout_orig,
			&stdout_log);
  run_filter(&ops,
	     stdin_orig, stdout_orig,
	     stdout_log);

  return EXIT_SUCCESS;
}
