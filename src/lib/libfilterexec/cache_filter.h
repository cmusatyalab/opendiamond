/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_CEVAL_PRIV_H_
#define	_CEVAL_PRIV_H_ 	1
/*
struct ceval_state;
 
typedef struct ceval_state {
	pthread_t	ceval_thread_id;   // thread for cache table
	filter_data_t * fdata;
	odisk_state_t * odisk;
	void * cookie;
	stats_drop stats_drop_fn;
	stats_drop stats_process_fn;
} ceval_state_t;
*/


int ceval_inject_names(char **nlist, int nents);


#endif	/* !_CEVAL_PRIV_H_ */

