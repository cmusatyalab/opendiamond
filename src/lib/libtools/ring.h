#ifndef	_RING_H_
#define	_RING_H_


#ifdef __cplusplus
extern "C" {
#endif


/* This number of elements in a ring.  This must be a multiple of
 * 2.
 */

typedef enum {
	RING_TYPE_SINGLE,
	RING_TYPE_DOUBLE
} ring_type_t;

typedef	struct ring_data {
	pthread_mutex_t	mutex;		/* make sure updates are atomic */
	ring_type_t	type;		/* is it 1 or 2 element ring ? */
	int		head;		/* location for next enq */
	int		tail;		/* location for next deq */
	int		size;		/* total number of elements */
	void *		data[0];
} ring_data_t;


#define	RING_STORAGE_SZ(__num_elem)	(sizeof(ring_data_t) + (((__num_elem) + 2) \
					 * sizeof(void *)))


/*
 * These are the function prototypes that are used for
 * single entries.
 */
extern int	ring_init(ring_data_t **ring, int num_elem);
extern int	ring_empty(ring_data_t *ring);
extern int	ring_enq(ring_data_t *ring, void *data);
extern int	ring_count(ring_data_t *ring);
extern void *	ring_deq(ring_data_t *ring);

/*
 * These are the function prototypes for the double entry functions.
 */
extern int	ring_2init(ring_data_t **ring, int num_elem);
extern int	ring_2empty(ring_data_t *ring);
extern int	ring_2enq(ring_data_t *ring, void *data1, void *data2);
extern int	ring_2count(ring_data_t *ring);
extern int 	ring_2deq(ring_data_t *ring, void **data1, void **data2);

#ifdef __cplusplus
}
#endif



#endif /* _RING_H_ */
