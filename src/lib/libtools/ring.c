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
