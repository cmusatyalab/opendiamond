#ifndef	_RING_H_
#define	_RING_H_


#ifdef __cplusplus
extern "C" {
#endif


/* This number of elements in a ring.  This must be a multiple of
 * 2.
 */
#define	RING_DATA_SIZE	1024

typedef enum {
	RING_TYPE_SINGLE,
	RING_TYPE_DOUBLE
} ring_type_t;

typedef	struct ring_data {
	pthread_mutex_t	mutex;		/* make sure updates are atomic */
	ring_type_t	type;		/* is it 1 or 2 element ring ? */
	int		head;		/* location for next enq */
	int		tail;		/* location for next deq */
	void *	data[RING_DATA_SIZE];
} ring_data_t;



/*
 * These are the function prototypes that are used for
 * single entries.
 */
extern int	ring_init(ring_data_t **ring);
extern int	ring_empty(ring_data_t *ring);
extern int	ring_enq(ring_data_t *ring, void *data);
extern void *	ring_deq(ring_data_t *ring);

/*
 * These are the function prototypes for the double entry functions.
 */
extern int	ring_2init(ring_data_t **ring);
extern int	ring_2empty(ring_data_t *ring);
extern int	ring_2enq(ring_data_t *ring, void *data1, void *data2);
extern int 	ring_2deq(ring_data_t *ring, void **data1, void **data2);

#ifdef __cplusplus
}
#endif



#endif /* _RING_H_ */
