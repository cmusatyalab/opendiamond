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

#ifndef	_OCACHE_PRIV_H_
#define	_OCACHE_PRIV_H_ 	1


struct ocache_state;


#define	MAX_DIR_PATH	512
#define	MAX_GID_NAME	128

#define	CACHE_EXT		"CACHEFL"
#define CACHE_OATTR_EXT		"OATTR"

#define	MAX_GID_FILTER	64
/*
 * XXX we need to clean up this interface so this is not externally 
 * visible.
 */
typedef struct ocache_state {
	char		ocache_path[MAX_DIR_PATH];
	pthread_t	c_thread_id;   // thread for cache table
	pthread_t	o_thread_id;   // thread for output attrs
	void *		dctl_cookie;
	void *		log_cookie;
} ocache_state_t;

void ceval_wattr_stats(off_t len);



#endif	/* !_OCACHE_PRIV_H_ */

