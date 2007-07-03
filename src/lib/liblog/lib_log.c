/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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
#include "lib_dctl.h"
#include "dctl_common.h"
#include "lib_log.h"

#define DCTL_NAME_LEN 128

static char const cvsid[] =
    "$Header$";

typedef struct log_state {
	unsigned int    level;
	unsigned int    type;
	pthread_mutex_t log_mutex;
	GQueue		   *buf;
	pthread_t		writer;
	char *			prefix;
	int				fd;
} log_state_t;

/*
 * some state for handling our multiple instantiations 
 */
static pthread_key_t log_state_key;
static pthread_once_t log_state_once = PTHREAD_ONCE_INIT;


/*
 * get the log state.
 */
static log_state_t *
log_get_state()
{
	log_state_t    *ls;

	ls = (log_state_t *) pthread_getspecific(log_state_key);
	// XXX ??? assert(ls != NULL);
	return (ls);
}


/*
 * Set the mask that corresponds to the level of events we are
 * willing to log.
 */

void
log_setlevel(unsigned int level_mask)
{
	log_state_t    *ls;
	/*
	 * If we haven't been initialized then there is nothing
	 * that we can do.
	 */
	ls = log_get_state();
	if (ls == NULL) {
		return;
	}

	ls->level = level_mask;
}


/*
 * Set the mask that corresponds to the type of events that
 * we are willing to log.
 */

void
log_settype(unsigned int type_mask)
{
	log_state_t    *ls;
	/*
	 * If we haven't been initialized then there is nothing
	 * that we can do.
	 */
	ls = log_get_state();
	if (ls == NULL) {
		return;
	}

	ls->type = type_mask;
}

static void
log_state_alloc()
{
	pthread_key_create(&log_state_key, NULL);
}

void
log_thread_register(void *cookie)
{
	pthread_setspecific(log_state_key, (char *) cookie);
}


/*
 * This will save the log message to the log buffer
 * that is currently being generated for this file as
 * long as this type of logging is enabled.
 */
void
log_message(unsigned int type, unsigned int level, char *fmt, ...)
{
	va_list         ap;
	va_list         new_ap;
	int             num;
	log_ent_t      *ent;
	log_state_t    *ls;

	ls = log_get_state();
	/*
	 * if ls == NULL, the we haven't initalized yet, so return 
	 */
	if (ls == NULL) {
		return;
	}

	/*
	 * If we aren't logging this level or type, then just return.
	 */
	if (((ls->type & type) == 0) || ((ls->level & level) == 0)) {
		return;
	}
	
	ent = (log_ent_t *) malloc(sizeof(log_ent_t));
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

	pthread_mutex_lock(&ls->log_mutex);
	g_queue_push_tail(ls->buf, (gpointer) ent);
	pthread_mutex_unlock(&ls->log_mutex);
}


/*
 * log_create - create a log file
 */
int log_create(char *prefix) {
	struct timeval now;
	int fd;
	int len;
	int offset = 0;
	struct tm *ltime;
	char path[MAXPATHLEN];
	
	gettimeofday(&now, NULL);
	// open the new file in the log dir
	len = sprintf(path, "%s/%s.", dconf_get_logdir(), prefix);
	offset += len;
	
	ltime = localtime(&now.tv_sec);
	strftime(&path[offset], MAX_LOG_ENTRY, "%b_%d_%y_%H:%M:%S.log", ltime);
	
	fd = open(path, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IRGRP|S_IROTH);
	return fd;
}


char level_to_char(uint32_t level) {
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


char source_to_char(uint32_t source) {
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
void *log_writer(void *arg) {
	log_ent_t		*ent;
	size_t			file_len = 0;
	size_t			wlen;
	int				offset;
	int             len;
	char			buf[MAX_LOG_ENTRY];
	struct tm	   *ltime;
	char			source;
	char			level;
	log_state_t	   *ls;
	
	ls = (log_state_t *) arg;
	ls->fd = log_create(ls->prefix);
	
	while (1) {
		pthread_mutex_lock(&ls->log_mutex);
		ent = (log_ent_t *) g_queue_pop_head(ls->buf);
		pthread_mutex_unlock(&ls->log_mutex);
		if (ent != NULL) {	
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
			wlen = write(ls->fd, buf, len);
			if (wlen > 0) {
				file_len += wlen;
			}
		}
		pthread_testcancel();
		g_usleep(G_USEC_PER_SEC);  /* wait one second */
	}
	/* NOTREACHED */
}

void log_init(char *log_prefix, char *control_prefix, void **cookie)
{
	log_state_t *ls;
	int		err;
	char log_path[DCTL_NAME_LEN];
	void *dc;
	
	pthread_once(&log_state_once, log_state_alloc);

	/*
	 * make sure we haven't be initialized more than once 
	 */
	if ((ls = pthread_getspecific(log_state_key)) != NULL) {
		*cookie = ls;		
		return;
	}

	ls = (log_state_t *) calloc(1, sizeof(*ls));
	if (ls == NULL) {
		/*
		 * XXX don't know what to do and who to report it to
		 */
		return;
	}
	ls->level = LOGL_CRIT|LOGL_ERR|LOGL_INFO;
	ls->type = LOGT_ALL;
	ls->buf = g_queue_new();
	ls->prefix = malloc(strlen(log_prefix)+1);
	strcpy(ls->prefix, log_prefix);
	err = pthread_mutex_init(&ls->log_mutex, NULL);

	/* set up dynamic control of log content */
        dctl_init(&dc);
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
	
	err = dctl_register_leaf(log_path, "log_level",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &ls->level);
	assert(err == 0);
	dctl_register_leaf(log_path, "log_type",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &ls->type);
	assert(err == 0);
				 
	pthread_setspecific(log_state_key, (char *) ls);
	err = pthread_create(&ls->writer, NULL, log_writer,	(void *) ls);
	*cookie = ls;
	
	assert(err == 0);
}


void log_term(void *cookie) {
	log_state_t *ls;

	ls = (log_state_t *) cookie;
	if (ls == NULL)
		return;
		
	pthread_cancel(ls->writer);
	close(ls->fd);
}

