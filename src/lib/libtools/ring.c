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


/* XXX locking for multi-threaded apps !! */

int
ring_init(ring_data_t **ring)
{
	int 		err;
	ring_data_t * 	new_ring;

	new_ring = (ring_data_t *) malloc(sizeof(*new_ring));
	if (new_ring == NULL) {
		*ring = NULL;
		return(ENOENT);
	}

	err = pthread_mutex_init(&new_ring->mutex, NULL);
	if (err) {
		free(new_ring);
		return(err);
	}

	new_ring->head = 0;
	new_ring->tail = 0;
	new_ring->type = RING_TYPE_SINGLE;

	*ring = new_ring;
	return(0);
}


int
ring_empty(ring_data_t *ring)
{
	assert(ring->type == RING_TYPE_SINGLE);
	if (ring->head == ring->tail) {
		return (1);
	} else {
		return (0);
	}	
}

int
ring_enq(ring_data_t *ring, void *data)
{
	int	new_head;

	assert(ring->type == RING_TYPE_SINGLE);

	pthread_mutex_lock(&ring->mutex);
	new_head = ring->head + 1;
	if (new_head >= RING_DATA_SIZE) {
		new_head = 0;
	}

	if (new_head == ring->tail) {
		/* XXX ring is full return error */
		/* XXX err code ??? */
		pthread_mutex_unlock(&ring->mutex);
		return (1);
	}

	ring->data[ring->head] = data;
	ring->head = new_head;
	pthread_mutex_unlock(&ring->mutex);
	return (0);
}

void *
ring_deq(ring_data_t *ring)
{
	void *		data;

	assert(ring->type == RING_TYPE_SINGLE);

	pthread_mutex_lock(&ring->mutex);
 	if (ring->head == ring->tail) {
		pthread_mutex_unlock(&ring->mutex);
		return(NULL);
	}		
	
	data = ring->data[ring->tail];
	ring->tail++;
	if (ring->tail >= RING_DATA_SIZE) {
		ring->tail = 0;
	}
	pthread_mutex_unlock(&ring->mutex);
	return(data);

}

/*
 * Initialize the "2-entry" ring.   We use the same ring structure,
 * we just advance the head/tail by twice as much.
 */
int
ring_2init(ring_data_t **ring)
{
	int 		err;
	ring_data_t * 	new_ring;

	new_ring = (ring_data_t *) malloc(sizeof(*new_ring));
	if (new_ring == NULL) {
		*ring = NULL;
		return(ENOENT);
	}

	err = pthread_mutex_init(&new_ring->mutex, NULL);
	if (err) {
		free(new_ring);
		return(err);
	}

	new_ring->head = 0;
	new_ring->tail = 0;
	new_ring->type = RING_TYPE_DOUBLE;

	*ring = new_ring;
	return(0);
}

/*
 * Test a 2-entry ring to see if there is data.  
 */

int
ring_2empty(ring_data_t *ring)
{
	assert(ring->type == RING_TYPE_DOUBLE);

	if (ring->head == ring->tail) {
		return (1);
	} else {
		return (0);
	}	
}

int
ring_2enq(ring_data_t *ring, void *data1, void *data2)
{
	int	new_head;

	assert(ring->type == RING_TYPE_DOUBLE);

	pthread_mutex_lock(&ring->mutex);

	new_head = ring->head + 2;
	if (new_head >= RING_DATA_SIZE) {
		new_head = 0;
	}

	if (new_head == ring->tail) {
		/* XXX ring is full return error */
		/* XXX err code ??? */
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
ring_2deq(ring_data_t *ring, void **data1, void **data2)
{

	assert(ring->type == RING_TYPE_DOUBLE);

	pthread_mutex_lock(&ring->mutex);
 	if (ring->head == ring->tail) {
		pthread_mutex_unlock(&ring->mutex);
		return(1);
	}		
	
	*data1 = ring->data[ring->tail];
	*data2 = ring->data[ring->tail + 1];
	ring->tail += 2;
	if (ring->tail >= RING_DATA_SIZE) {
		ring->tail = 0;
	}
	pthread_mutex_unlock(&ring->mutex);
	return(0);

}



