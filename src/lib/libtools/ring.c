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

/*
 * This provides the functions for accessing the rings
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include "ring.h"

static char const cvsid[] =
    "$Header$";

/*
 * XXX move to common library 
 */
static double
tv_to_float(struct timeval *tv)
{
	double          tmp;

	tmp = (double) tv->tv_usec;
	tmp /= 1000000.0;
	tmp += ((double) tv->tv_sec);
	// XXX printf("tvtfloat: %ld.%ld -> %f \n", tv->tv_sec, tv->tv_usec,
	// tmp);
	return (tmp);
}

static double
get_float_time()
{
	double          ftime;
	struct timeval  tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);

	ftime = tv_to_float(&tv);
	return (ftime);
}

static double
new_rate(double old_rate, double cur_rate)
{
	double          nrate;

	nrate =
	    (((double) (RATE_AVG_WINDOW - 1)) / ((double) (RATE_AVG_WINDOW)) *
	     old_rate);
	nrate += cur_rate / ((double) (RATE_AVG_WINDOW));

	return (nrate);
}


int
ring_enq_idx(ring_data_t * ring)
{
	pthread_t       self = pthread_self();
	int             i;

	for (i = 0; i < MAX_ENQ_THREAD; i++) {
		if (ring->en_state[i].thread_id == self) {
			return (i);
		} else if (ring->en_state[i].thread_id == 0) {
			ring->en_state[i].thread_id = self;
			return (i);
		}
	}
	return (-1);

}

static void
ring_update_erate(ring_data_t * ring)
{
	double          cur_time;
	double          nrate;
	int             idx;

	idx = ring_enq_idx(ring);
	if (idx < 0) {
		return;
	}

	cur_time = get_float_time();

	if (ring->en_state[idx].last_enq != 0.0) {
		nrate = 1.0 / (cur_time - ring->en_state[idx].last_enq);
		// printf("erate: curt %f laste %f rate %f\n", cur_time,
		// ring->last_enq,
		// nrate);
		ring->enq_rate = new_rate(ring->enq_rate, nrate);
	}

	ring->en_state[idx].last_enq = cur_time;
}

static void
ring_update_drate(ring_data_t * ring)
{
	double          cur_time;
	double          nrate;

	cur_time = get_float_time();

	if (ring->last_deq != 0.0) {
		nrate = 1.0 / (cur_time - ring->last_deq);
		ring->deq_rate = new_rate(ring->deq_rate, nrate);
	}

	ring->last_deq = cur_time;
}




/*
 * XXX locking for multi-threaded apps !! 
 */

int
ring_init(ring_data_t ** ring, int num_elems)
{
	int             err;
	int             size;
	int             i;
	ring_data_t    *new_ring;

	/*
	 * XXX check ring is power of 2 
	 */
	size = RING_STORAGE_SZ(num_elems);
	new_ring = (ring_data_t *) malloc(size);
	if (new_ring == NULL) {
		*ring = NULL;
		return (ENOENT);
	}

	err = pthread_mutex_init(&new_ring->mutex, NULL);
	if (err) {
		free(new_ring);
		return (err);
	}

	new_ring->head = 0;
	new_ring->tail = 0;
	new_ring->type = RING_TYPE_SINGLE;
	new_ring->size = num_elems;

	new_ring->enq_rate = 0.0;
	new_ring->deq_rate = 0.0;
	new_ring->last_deq = 0.0;

	for (i = 0; i < MAX_ENQ_THREAD; i++) {
		new_ring->en_state[i].thread_id = 0;
		new_ring->en_state[i].last_enq = 0.0;
	}

	*ring = new_ring;
	return (0);
}


int
ring_empty(ring_data_t * ring)
{
	assert(ring->type == RING_TYPE_SINGLE);
	if (ring->head == ring->tail) {
		/*
		 * assume output stall so deq rate will be broken 
		 */
		ring->last_deq = 0.0;
		return (1);
	} else {
		return (0);
	}
}

int
ring_full(ring_data_t * ring)
{
	int             new_head;
	assert(ring->type == RING_TYPE_SINGLE);

	new_head = ring->head + 1;
	if (new_head >= ring->size) {
		new_head = 0;
	}

	if (new_head == ring->tail) {
		/*
		 * assume input stall so enq rate will be broken 
		 */
		int             idx = ring_enq_idx(ring);
		if (idx >= 0) {
			ring->en_state[idx].last_enq = 0.0;
		}
		return (1);
	} else {
		return (0);
	}
}

int
ring_count(ring_data_t * ring)
{
	int             diff;

	assert(ring->type == RING_TYPE_SINGLE);

	if (ring->head >= ring->tail) {
		diff = ring->head - ring->tail;
	} else {
		diff = (ring->head + ring->size) - ring->tail;
	}

	assert(diff >= 0);
	assert(diff <= ring->size);

	return (diff);
}


int
ring_enq(ring_data_t * ring, void *data)
{
	int             new_head;

	assert(ring->type == RING_TYPE_SINGLE);

	pthread_mutex_lock(&ring->mutex);
	new_head = ring->head + 1;
	if (new_head >= ring->size) {
		new_head = 0;
	}

	if (new_head == ring->tail) {
		/*
		 * XXX ring is full return error 
		 */
		/*
		 * XXX err code ??? 
		 */
		int             idx = ring_enq_idx(ring);
		if (idx >= 0) {
			ring->en_state[idx].last_enq = 0.0;
		}
		pthread_mutex_unlock(&ring->mutex);
		return (1);
	}

	ring->data[ring->head] = data;
	ring->head = new_head;
	ring_update_erate(ring);
	pthread_mutex_unlock(&ring->mutex);
	return (0);
}

void           *
ring_deq(ring_data_t * ring)
{
	void           *data;

	assert(ring->type == RING_TYPE_SINGLE);

	pthread_mutex_lock(&ring->mutex);
	if (ring->head == ring->tail) {
		ring->last_deq = 0.0;
		pthread_mutex_unlock(&ring->mutex);
		return (NULL);
	}

	data = ring->data[ring->tail];
	ring->tail++;
	if (ring->tail >= ring->size) {
		ring->tail = 0;
	}
	ring_update_drate(ring);
	pthread_mutex_unlock(&ring->mutex);
	return (data);
}


float
ring_erate(ring_data_t * ring)
{
	float           erate;
	assert(ring->type == RING_TYPE_SINGLE);
	pthread_mutex_lock(&ring->mutex);
	erate = (float) ring->enq_rate;
	pthread_mutex_unlock(&ring->mutex);
	return (erate);
}

float
ring_drate(ring_data_t * ring)
{
	float           drate;
	assert(ring->type == RING_TYPE_SINGLE);
	pthread_mutex_lock(&ring->mutex);
	drate = (float) ring->deq_rate;
	pthread_mutex_unlock(&ring->mutex);
	return (drate);
}

/*
 * Initialize the "2-entry" ring.   We use the same ring structure,
 * we just advance the head/tail by twice as much.
 */
int
ring_2init(ring_data_t ** ring, int num_elems)
{
	int             err;
	ring_data_t    *new_ring;
	int             total_ents;
	int             size;
	int             i;

	total_ents = num_elems * 2;
	size = RING_STORAGE_SZ(total_ents);

	new_ring = (ring_data_t *) malloc(size);
	if (new_ring == NULL) {
		*ring = NULL;
		return (ENOENT);
	}

	err = pthread_mutex_init(&new_ring->mutex, NULL);
	if (err) {
		free(new_ring);
		return (err);
	}

	new_ring->head = 0;
	new_ring->tail = 0;
	new_ring->type = RING_TYPE_DOUBLE;
	new_ring->size = total_ents;

	new_ring->enq_rate = 0.0;
	new_ring->deq_rate = 0.0;
	new_ring->last_deq = 0.0;

	for (i = 0; i < MAX_ENQ_THREAD; i++) {
		new_ring->en_state[i].thread_id = 0;
		new_ring->en_state[i].last_enq = 0.0;
	}
	*ring = new_ring;
	return (0);
}

/*
 * Test a 2-entry ring to see if there is data.  
 */

int
ring_2empty(ring_data_t * ring)
{
	assert(ring->type == RING_TYPE_DOUBLE);

	if (ring->head == ring->tail) {
		/*
		 * assume output stall so deq rate will be broken 
		 */
		ring->last_deq = 0.0;
		return (1);
	} else {
		return (0);
	}
}

int
ring_2count(ring_data_t * ring)
{
	int             diff;

	assert(ring->type == RING_TYPE_DOUBLE);

	if (ring->head >= ring->tail) {
		diff = ring->head - ring->tail;
	} else {
		diff = (ring->head + ring->size) - ring->tail;
	}

	diff = diff / 2;

	assert(diff >= 0);
	assert(diff <= (ring->size / 2));

	return (diff);
}

int
ring_2enq(ring_data_t * ring, void *data1, void *data2)
{
	int             new_head;

	assert(ring->type == RING_TYPE_DOUBLE);

	pthread_mutex_lock(&ring->mutex);

	new_head = ring->head + 2;
	if (new_head >= ring->size) {
		new_head = 0;
	}

	if (new_head == ring->tail) {
		/*
		 * XXX ring is full return error 
		 */
		/*
		 * XXX err code ??? 
		 */
		pthread_mutex_unlock(&ring->mutex);
		return (1);
	}

	ring->data[ring->head] = data1;
	ring->data[ring->head + 1] = data2;
	ring->head = new_head;
	ring_update_erate(ring);
	pthread_mutex_unlock(&ring->mutex);
	return (0);
}

float
ring_2erate(ring_data_t * ring)
{
	float           erate;
	assert(ring->type == RING_TYPE_DOUBLE);
	pthread_mutex_lock(&ring->mutex);
	erate = (float) ring->enq_rate;
	pthread_mutex_unlock(&ring->mutex);
	return (erate);
}

float
ring_2drate(ring_data_t * ring)
{
	float           drate;
	assert(ring->type == RING_TYPE_DOUBLE);
	pthread_mutex_lock(&ring->mutex);
	drate = (float) ring->deq_rate;
	pthread_mutex_unlock(&ring->mutex);
	return (drate);
}



int
ring_2deq(ring_data_t * ring, void **data1, void **data2)
{

	assert(ring->type == RING_TYPE_DOUBLE);

	pthread_mutex_lock(&ring->mutex);
	if (ring->head == ring->tail) {
		pthread_mutex_unlock(&ring->mutex);
		return (1);
	}

	*data1 = ring->data[ring->tail];
	*data2 = ring->data[ring->tail + 1];
	ring->tail += 2;
	if (ring->tail >= ring->size) {
		ring->tail = 0;
	}
	ring_update_drate(ring);
	pthread_mutex_unlock(&ring->mutex);
	return (0);

}
