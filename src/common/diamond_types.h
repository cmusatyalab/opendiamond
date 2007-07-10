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

#ifndef _DIAMOND_TYPES_H_
#define	_DIAMOND_TYPES_H_

#include <stdint.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "rtimer.h"
#include "diamond_consts.h"


#ifdef __cplusplus
extern "C"
{
#endif

typedef uint64_t    groupid_t;

struct obj_id {
	uint64_t    dev_id;
	uint64_t    local_id;
};

typedef struct obj_id obj_id_t;

/*
 * The handle to a current search context.
 */

typedef	void *	ls_search_handle_t;


/*
 * The handle to a current object.
 */
typedef	void *	ls_obj_handle_t;

/*
 * The handle to a current object. (XXX)
 */
typedef	void *	ls_dev_handle_t;


/*
 * This is the structure that returns the statistics associated with
 * each of the filters.  It returns the name of the filter and
 * some relevant stats.
 *
 * These stats will most likely be used for profiling and building
 * progress meters.  The application may not need to use them.
 */

typedef struct filter_stats {
	char	fs_name[MAX_FILTER_NAME];  /* the filter name */
	int		fs_objs_processed;	   /* objs processed by filter*/
	int		fs_objs_dropped;	   /* obj dropped by filter */
	/* JIAYING */
	int		fs_objs_cache_dropped;
	int		fs_objs_cache_passed;
	int		fs_objs_compute;
	/* JIAYING */
	int		fs_hits_inter_session;	/* hits computed before this session */
	int		fs_hits_inter_query;	/* hits computed in session, before query */
	int		fs_hits_intra_query;	/* hits computed this query */
	rtime_t		fs_avg_exec_time;	   /* avg time spent in filter*/
} filter_stats_t;


/*
 *  This structure is used to return the device statistics during the execution
 *  of a searchlet.  This is a variable length data structure that depends the 
 *  number of filter executing at the device, the actual number of entries
 *  at the end will be determined by the field ds_num_filters.
 * 
 *  These will primarily be used for profiling and monitoring progres.
 */
typedef struct dev_stats {
	int		ds_objs_total;	   	/* total objs in search  */
	int		ds_objs_processed;	/* total objects by device */
	int		ds_objs_dropped;	/* total objects dropped */
	int		ds_objs_nproc;		/* objs not procced at disk */
	int		ds_system_load;		/* average load on  device??? */
	rtime_t	ds_avg_obj_time;	/* average time per objects */
	int		ds_num_filters; 	/* number of filters */
	filter_stats_t	ds_filter_stats[0];	/* list of filter */
} dev_stats_t;


typedef struct {
  int len;
  char **names;
  double *values;
} device_session_vars_t;


/* copy from lib_log.h */
#ifndef offsetof
#define offsetof(type, member) ( (int) & ((type*)0) -> member )
#endif

#define DEV_STATS_BASE_SIZE  (offsetof(struct dev_stats, ds_filter_stats))
#define DEV_STATS_SIZE(nfilt) \
  (DEV_STATS_BASE_SIZE + (sizeof(filter_stats_t) * (nfilt)))


typedef struct host_stats {
	int		hs_objs_received;	/* objects at the host, all devices (obj_ring) */
	int 	hs_objs_queued;		/* objects queued for ls_next_object (proc_ring) */
	int		hs_objs_read;		/* objects returned through ls_next_object */
	int		hs_objs_uqueued;	/* objects queued for user */
	int		hs_objs_upresented;	/* objects presented to user */
} host_stats_t;

typedef struct app_stats {
	int		as_objs_queued;		/* objects queued for user */
	int		as_objs_presented;	/* objects presented to user */
} app_stats_t;

/*
 * This is an enumeration for the instruction set of the processor on
 * the storage device.  
 * 
 * XXX these are just placeholder types.
 */

typedef enum {
    DEV_ISA_UNKNOWN = 0,
    DEV_ISA_IA32,
    DEV_ISA_IA64,
    DEV_ISA_XSCALE,
} device_isa_t;

/*
 * Enumeration for user state hint
 */
typedef enum {
	USER_UNKNOWN,
	USER_WAITING,
	USER_BUSY,
} user_state_t;

/*
 * descriptor for search sessions
 */
typedef struct {
	struct sockaddr_in 	clientaddr;
	struct timeval		connect_time;
	int					conn_idx;
} session_info_t;

/*
 * descriptor for individual queries
 */
typedef struct {
	session_info_t	session;
	int				query_id;
} query_info_t;


/*
 * The device characteristics.
 * XXX this is a placehold and will likely need to be exanded.
 */

typedef struct device_char {
    device_isa_t	dc_isa;		/* instruction set of the device    */
    uint64_t		dc_speed;	/* CPU speed, (some bogomips, etc.) */
    uint64_t		dc_mem;		/* Available memory for the system  */
    uint32_t		dc_devid;	/* Available memory for the system  */
} device_char_t;


/* for anomaly detection */
typedef struct {
  double local_val;
  double global_val;
} session_variable_value_t;



#ifdef __cplusplus
}
#endif

#endif /* !_DIAMOND_TYPES_H_ */

