#ifndef	_CEVAL_PRIV_H_
#define	_CEVAL_PRIV_H_ 	1

struct ceval_state;

typedef struct ceval_state {
	pthread_t	ceval_thread_id;   // thread for cache table
	filter_data_t * fdata;
	odisk_state_t * odisk;
	void * cookie;
	stats_drop stats_drop_fn;
	stats_drop stats_process_fn;
} ceval_state_t;

typedef struct obj_eval {
} obj_eval_t;

#endif	/* !_CEVAL_PRIV_H_ */

