#ifndef	_OCACHE_PRIV_H_
#define	_OCACHE_PRIV_H_ 	1


struct ocache_state;


#define	MAX_DIR_PATH	128
#define	MAX_GID_NAME	128

#define	CACHE_EXT		"CACHEFL"
#define CACHE_DIR		"cache"
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


#endif	/* !_OCACHE_PRIV_H_ */

