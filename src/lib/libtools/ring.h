#ifndef	_RING_H_
#define	_RING_H_

#define	RING_DATA_SIZE	1024


typedef	struct ring_data {
	pthread_mutex_t	mutex;
	int	head;			/* location for next enq */
	int	tail;			/* location for next deq */
	void *	data[RING_DATA_SIZE];
} ring_data_t;
	

extern int	ring_init(ring_data_t **ring);
extern int	ring_empty(ring_data_t *ring);
extern int	ring_enq(ring_data_t *ring, void *data);
extern void *	ring_deq(ring_data_t *ring);


#endif /* _RING_H_ */
