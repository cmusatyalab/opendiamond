/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DIAMOND_TYPES_H_
#define	_DIAMOND_TYPES_H_ 

#include <stdint.h>
#include "rtimer.h"


#ifdef __cplusplus
extern "C" {
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
	char		fs_name[MAX_FILTER_NAME];  /* the filter name */
	int		fs_objs_processed;	   /* objs processed by filter*/
	int		fs_objs_dropped;	   /* obj dropped by filter */
/* JIAYING */
	int		fs_objs_cache_dropped;	   
	int		fs_objs_cache_passed;	   
	int		fs_objs_compute;	   
/* JIAYING */
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
	rtime_t		ds_avg_obj_time;	/* average time per objects */
	int		ds_num_filters; 	/* number of filters */
	filter_stats_t	ds_filter_stats[0];	/* list of filter */
} dev_stats_t;

/* copy from lib_log.h */
#ifndef offsetof
#define offsetof(type, member) ( (int) & ((type*)0) -> member )
#endif

#define DEV_STATS_BASE_SIZE  (offsetof(struct dev_stats, ds_filter_stats))
#define DEV_STATS_SIZE(nfilt) \
  (DEV_STATS_BASE_SIZE + (sizeof(filter_stats_t) * (nfilt)))

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
 * The device characteristics.
 * XXX this is a placehold and will likely need to be exanded.
 */

typedef struct device_char {
	device_isa_t	dc_isa;		/* instruction set of the device    */
	uint64_t		dc_speed;	/* CPU speed, (some bogomips, etc.) */
	uint64_t		dc_mem;		/* Available memory for the system  */
	uint32_t		dc_devid;	/* Available memory for the system  */
} device_char_t;


#ifdef __cplusplus
}
#endif

#endif /* !_DIAMOND_TYPES_H_ */

