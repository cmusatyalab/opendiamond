/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This provides the functions for accessing the rings
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "ring.h"
#include <assert.h>


/*
 * XXX locking for multi-threaded apps !! 
 */

int
ring_init(ring_data_t ** ring, int num_elems)
{
	int             err;
	int             size;
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

	*ring = new_ring;
	return (0);
}


int
ring_empty(ring_data_t * ring)
{
	assert(ring->type == RING_TYPE_SINGLE);
	if (ring->head == ring->tail) {
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
		pthread_mutex_unlock(&ring->mutex);
		return (1);
	}

	ring->data[ring->head] = data;
	ring->head = new_head;
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
		pthread_mutex_unlock(&ring->mutex);
		return (NULL);
	}

	data = ring->data[ring->tail];
	ring->tail++;
	if (ring->tail >= ring->size) {
		ring->tail = 0;
	}
	pthread_mutex_unlock(&ring->mutex);
	return (data);

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
	pthread_mutex_unlock(&ring->mutex);

	return (0);
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
	pthread_mutex_unlock(&ring->mutex);
	return (0);

}
