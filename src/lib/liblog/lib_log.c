/*
 *      Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
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
#include "lib_log.h"


static char const cvsid[] =
    "$Header$";

typedef struct log_state {
	int             head;
	int             tail;
	int             drops;
	unsigned int    level;
	unsigned int    type;
	pthread_mutex_t log_mutex;
	char            buffer[MAX_LOG_BUFFER];
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

void
log_init(void **cookie)
{


	pthread_once(&log_state_once, log_state_alloc);

	/*
	 * make sure we haven't be initialized more than once 
	 */
	if (pthread_getspecific(log_state_key) != NULL) {
		assert(0);
	}

	ls = (log_state_t *) malloc(sizeof(*ls));
	if (ls == NULL) {
		/*
		 * XXX don't know what to do and who to report it to
		 */
		return;
	}
	memset(ls, 0, sizeof(*ls));
	ls->head = 0;
	ls->tail = 0;
	ls->drops = 0;
	ls->level = LOGL_CRIT|LOGL_ERR;
	ls->type = LOGT_ALL;

	err = pthread_mutex_init(&ls->log_mutex, NULL);
	pthread_setspecific(log_state_key, (char *) ls);
	*cookie = ls;

	assert(err == 0);
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
	int             remain;
	int             total_len;
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
	 * If we aren't loggin this level or type, then just return.
	 */
	if (((ls->type & type) == 0) || ((ls->level & level) == 0)) {
		return;
	}


	/*
	 * see if there is enough room for this entry.  We do a little
	 * tricky math here.  The only cases where it is a problem is
	 * where head < tail and tail-head < MAX_LOG_ENTRY.  If head > tail,
	 * then we make sure there is enough room at the end of the
	 * log otherwise we would have advanced the pointer back to the 
	 * begining when we did the alignment thing.
	 */

	pthread_mutex_lock(&ls->log_mutex);
	assert((MAX_LOG_BUFFER - ls->head) >= MAX_LOG_ENTRY);

	if (ls->head == -1) {
		if (ls->tail != 0) {
			ls->head = 0;
		} else {
			pthread_mutex_unlock(&ls->log_mutex);
			return;
		}
	}

	remain = ls->tail - ls->head;
	if ((remain < MAX_LOG_ENTRY) && (remain > 0)) {
		ls->drops++;
		pthread_mutex_unlock(&ls->log_mutex);
		return;
	}

	ent = (log_ent_t *) & ls->buffer[ls->head];

	va_start(ap, fmt);
	va_copy(new_ap, ap);
	num = vsnprintf(&ent->le_data[0], MAX_LOG_STRING, fmt, new_ap);
	va_end(ap);

	/*
	 * include the trailing '\0' 
	 */
	num++;

	/*
	 * deal with the case that num is the number that may have
	 * been written not the number written in the case of
	 * truncation.
	 */
	if ((num > MAX_LOG_STRING) || (num == -1)) {
		num = MAX_LOG_STRING;
	}



	/*
	 * store the data associated with this record 
	 */
	ent->le_dlen = htonl(num);
	ent->le_level = htonl(level);
	ent->le_type = ntohl(type);

	/*
	 * Round up the total length to a sizeof an int to make sure
	 * that to make sure that data structure stays nicely
	 * aligned.
	 */

	total_len = LOG_ENT_BASE_SIZE + num;
	total_len = (total_len + (sizeof(int) - 1)) & ~(sizeof(int) - 1);

	/*
	 * advance the header pointer, as we checked above, we should
	 * not need to worry about the wrap because we are guaranteed to have
	 * more that enough space here.
	 */

	ls->head += total_len;

	/*
	 * If there is not enough room at the end for another entry, then
	 * move back around. 
	 */
	remain = MAX_LOG_BUFFER - ls->head;

	/*
	 * If we end the buffer set head to -1, we deal with this
	 * case at the beggining of this loop.
	 */
	if (remain < MAX_LOG_ENTRY) {
		ls->head = -1;
		total_len = total_len + remain;
	}

	ent->le_nextoff = htonl(total_len);

	pthread_mutex_unlock(&ls->log_mutex);
}



int
log_getbuf(char **data)
{
	int             len;
	log_state_t    *ls;

	ls = log_get_state();
	/*
	 * if they are equal we have no more data
	 */
	if (ls->tail == ls->head) {
		return (0);
	}

	if (ls->tail < ls->head) {
		len = ls->head - ls->tail;
	} else {
		len = MAX_LOG_BUFFER - ls->tail;
	}


	*data = &ls->buffer[ls->tail];

	return (len);
}



/*
 * There isn't a lot we can do from here to validate
 */
void
log_advbuf(int len)
{
	log_state_t    *ls;

	ls = log_get_state();

	pthread_mutex_lock(&ls->log_mutex);

	ls->tail += len;
	if (ls->tail >= MAX_LOG_BUFFER) {
		ls->tail = 0;
		if (ls->head == -1) {
			ls->head = 0;
		}
	}

	pthread_mutex_unlock(&ls->log_mutex);
}
