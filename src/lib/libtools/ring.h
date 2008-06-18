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

#ifndef	_RING_H_
#define	_RING_H_

#include <pthread.h>
#include <diamond_features.h>

#ifdef __cplusplus
extern          "C"
{
#endif


#define	RATE_AVG_WINDOW		64

#define	MAX_ENQ_THREAD		8

/* thread specific enqueue state */
typedef struct enq_state {
	pthread_t		thread_id;
	double			last_enq;	/* last enqueue time */
} enq_state_t;

typedef struct ring_data {
	pthread_mutex_t mutex;  /* make sure updates are atomic */
	int             head;   /* location for next enq */
	int             tail;   /* location for next deq */
	enq_state_t	en_state[MAX_ENQ_THREAD];
	double		enq_rate;	/* enqueue rate in objs/sec */
	double		deq_rate;	/* dequeue rate in objs/sec */
	double		last_deq;	/* last dequeue time */
	int             size;   /* total number of elements */
	void           *data[0];
} ring_data_t;


#define	RING_STORAGE_SZ(__num_elem)	(sizeof(ring_data_t) + (((__num_elem) + 2) \
			 * sizeof(void *)))


/*
* These are the function prototypes that are used for
 * single entries.
 */
diamond_public
int             ring_init(ring_data_t ** ring, int num_elem);

diamond_public
int             ring_empty(ring_data_t * ring);
int             ring_full(ring_data_t * ring);

diamond_public
int             ring_enq(ring_data_t * ring, void *data);

diamond_public
int             ring_count(ring_data_t * ring);
float           ring_erate(ring_data_t * ring);
float           ring_drate(ring_data_t * ring);

diamond_public
void           *ring_deq(ring_data_t * ring);

#ifdef __cplusplus
}
#endif
#endif                          /* _RING_H_ */

