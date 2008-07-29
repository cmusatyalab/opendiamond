/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <glib.h>

#include "diamond_types.h"
#include "lib_dconfig.h"
#include "dctl_common.h"
#include "dctl_impl.h"
#include "lib_log.h"
#include "dconfig_priv.h"

#define DCTL_NAME_LEN 128

typedef struct log_ent {
	struct timeval le_ts;   /* timestamp */
	pid_t		le_pid;		/* process id */
	pthread_t	le_tid; 	/* thread id */
	uint32_t 	le_level;	/* the level */
	uint32_t 	le_type;	/* the type */
	uint32_t 	le_dlen;	/* length of data */
	char		le_data[MAX_LOG_ENTRY];	/* where the string is stored */
} log_ent_t;


typedef struct log_state {
  unsigned int    level;
  unsigned int    type;
  GAsyncQueue *   queue;
  pthread_t	  writer;
  char *          prefix;
  int             fd;
} log_state_t;

/* our 1 static state */
static log_state_t *log_state;

/* special EOF symbol */
static const void *log_eof = &log_eof;

/*
 * Set the mask that corresponds to the level of events we are
 * willing to log.
 */

void
log_setlevel(unsigned int level_mask)
{
  if (log_state == NULL) {
    return;
  }
  log_state->level = level_mask;
}


/*
 * Set the mask that corresponds to the type of events that
 * we are willing to log.
 */

void
log_settype(unsigned int type_mask)
{
  if (log_state == NULL) {
    return;
  }

  log_state->type = type_mask;
}

/*
 * This will save the log message to the log buffer
 * that is currently being generated for this file as
 * long as this type of logging is enabled.
 */
void
log_message(unsigned int type, unsigned int level, const char *fmt, ...)
{
	va_list         ap;
	va_list         new_ap;
	int             num;
	log_ent_t      *ent;

	/*
	 * if log_state == NULL, the we haven't initalized yet, so return 
	 */
	if (log_state == NULL) {
		return;
	}

	/*
	 * If we aren't logging this level or type, then just return.
	 */
	if (((log_state->type & type) == 0) || ((log_state->level & level) == 0)) {
		return;
	}
	
	ent = (log_ent_t *) malloc(sizeof(*ent));
	gettimeofday(&ent->le_ts, NULL);
	ent->le_pid = getpid();
	ent->le_tid = pthread_self();
	ent->le_level = level;
	ent->le_type = type;
	
	va_start(ap, fmt);
	va_copy(new_ap, ap);
	num = vsnprintf(&ent->le_data[0], MAX_LOG_ENTRY, fmt, new_ap);
	va_end(ap);
	
	/*
	 * clobber the '\0' 
	 */
	if (num < MAX_LOG_ENTRY-1) {
		ent->le_data[num++] = ' ';
	}

	/*
	 * deal with the case that num is the number that may have
	 * been written not the number written in the case of
	 * truncation.
	 */
	if ((num > MAX_LOG_ENTRY) || (num == -1)) {
		num = MAX_LOG_ENTRY;
	}
	ent->le_dlen = num;

	g_async_queue_push(log_state->queue, (gpointer) ent);
}


/*
 * log_create - create a log file
 */
static int log_create(char *prefix) {
	struct timeval now;
	int fd;
	int len;
	int offset = 0;
	struct tm *ltime;
	char path[MAXPATHLEN];
	char *logdir = dconf_get_logdir();

	gettimeofday(&now, NULL);
	// open the new file in the log dir
	len = sprintf(path, "%s/%s.", logdir, prefix);
	free(logdir);
	offset += len;
	
	ltime = localtime(&now.tv_sec);
	strftime(&path[offset], MAX_LOG_ENTRY, "%b_%d_%y_%H:%M:%S.log", ltime);
	
	fd = open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IRGRP|S_IROTH);
	return fd;
}


static char level_to_char(uint32_t level) {
	char c;
	
	switch(level) {
		case LOGL_CRIT:
			c = 'C';
			break;

		case LOGL_ERR:
			c = 'E'; 
			break;

		case LOGL_INFO:
			c = 'I'; 
			break;

		case LOGL_TRACE:
			c = 'T';
			break;

		case LOGL_DEBUG:
			c = 'D';
			break;

		default:
			c = '?';
			break;
	}
	
	return(c);
}


static char source_to_char(uint32_t source) {
	char c;
	
	switch(source) {
		case LOGT_APP:
			c = 'A';
			break;

		case LOGT_DISK:
			c = 'D';
			break;

		case LOGT_FILT:
			c = 'F';
			break;

		case LOGT_BG:
			c = 'B'; 
			break;

		default:
			c = '?';
			break;
	}
	
	return(c);
}


/*
 * log_writer - main loop logging thread
 */
static void *log_writer(void *arg) {
	log_ent_t		*ent;
	size_t			file_len = 0;
	size_t			wlen;
	int				offset;
	int             len;
	char			buf[MAX_LOG_ENTRY];
	struct tm	   *ltime;
	char			source;
	char			level;
	
	log_state->fd = log_create(log_state->prefix);
	
	while (1) {
		// block on queue, waiting for message
		ent = (log_ent_t *) g_async_queue_pop(log_state->queue);

		if (ent == log_eof) {
		  // shutdown message received
		  break;
		}

		// real data received
		offset = 0;
		ltime = localtime(&ent->le_ts.tv_sec);
		len = strftime(&buf[0], MAX_LOG_ENTRY,
			       "\n%b %d %y %H:%M:%S", ltime);
		offset += len;

		source = source_to_char(ent->le_type);
		level = level_to_char(ent->le_level);
		len = snprintf(&buf[offset], MAX_LOG_ENTRY-offset,
			       ".%06d [%d.%u] %c %c ",
			       (int) ent->le_ts.tv_usec,
			       (int) ent->le_pid,
			       (int) ent->le_tid,
			       source, level);
		offset += len;

		if (offset + ent->le_dlen > MAX_LOG_ENTRY) {
		  ent->le_dlen = MAX_LOG_ENTRY-offset;
		}
		strncpy(&buf[offset], &ent->le_data[0], ent->le_dlen);
		len = offset + ent->le_dlen;
		/* message is ready */

		/* check the file size */
		if (file_len + len > MAX_LOG_FILE_SIZE) {
		  /* XXX roll the log over here */
		}

		/* write the record text */
		wlen = write(log_state->fd, buf, len);
		if (wlen > 0) {
		  file_len += wlen;
		}

		/* free the now-unused log structure malloc'd in log_message() */
		free(ent);
	}

	// we are done
	close(log_state->fd);
	return NULL;
}

void log_init(char *log_prefix, char *control_prefix)
{
	int		err;
	char log_path[DCTL_NAME_LEN];
	
	/*
	 * make sure we haven't be initialized more than once 
	 */
	if (log_state != NULL) {
		return;
	}

	if (!g_thread_supported()) g_thread_init(NULL);


	log_state = (log_state_t *) calloc(1, sizeof(*log_state));
	if (log_state == NULL) {
		/*
		 * XXX don't know what to do and who to report it to
		 */
		return;
	}
	log_state->level = LOGL_CRIT|LOGL_ERR|LOGL_INFO;
	log_state->type = LOGT_ALL;
	log_state->queue = g_async_queue_new();
	log_state->prefix = malloc(strlen(log_prefix)+1);
	strcpy(log_state->prefix, log_prefix);

	/* set up dynamic control of log content */
	if (control_prefix == NULL ||
		strcmp(control_prefix, ROOT_PATH) == 0) {
	  err = dctl_register_node(ROOT_PATH, LOG_PATH);
	  assert(err == 0);
	  err = dctl_register_node(LOG_PATH, log_prefix);
	  assert(err == 0);
	  snprintf(log_path, DCTL_NAME_LEN, "%s.%s", LOG_PATH, log_prefix);
	} else {
	  err = dctl_register_node(control_prefix, LOG_PATH);
	  assert(err == 0);
	  snprintf(log_path, DCTL_NAME_LEN, "%s.%s", 
		   control_prefix, LOG_PATH);
	  err = dctl_register_node(log_path, log_prefix);
	  assert(err == 0);
	  snprintf(log_path, DCTL_NAME_LEN, "%s.%s.%s", 
		   control_prefix, LOG_PATH, log_prefix);
	}

	dctl_register_u32(log_path, "log_level", O_RDWR, &log_state->level);
	dctl_register_u32(log_path, "log_type", O_RDWR, &log_state->type);

	/* start the thread */
	err = pthread_create(&log_state->writer, NULL, log_writer, NULL);
	assert(err == 0);
}


void log_term(void) {
	if (log_state == NULL) 
		return;

	// shutdown the thread and wait
	g_async_queue_push(log_state->queue, (gpointer)log_eof);
	pthread_join(log_state->writer, NULL);

	g_async_queue_unref(log_state->queue);
	free(log_state->prefix);
	free(log_state);
	log_state = NULL;
}
