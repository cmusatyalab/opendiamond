/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _ID_SEARCH_PRIV_H_
#define _ID_SEARCH_PRIV_H_


/*
 * This is header file defines the internal state that is used
 * for the searchlet library.
 */


/* Some constants */
#define	LS_OBJ_PEND_HW	60
#define	LS_OBJ_PEND_LW	55

/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */
#define	DEV_FLAG_RUNNING		0x01	
#define	DEV_FLAG_COMPLETE		0x02	
#define	DEV_FLAG_BLOCKED		0x04
struct search_state;

#define	DEFAULT_CREDIT_INCR		8.0
#define	MAX_CREDIT_INCR			20.0
#define	MAX_CUR_CREDIT			100.0

#define	DEFAULT_QUEUE_LEN		10

#define	MAX_DEV_GROUPS		64

typedef struct device_handle {
	struct device_handle * 			next;
	uint32_t				dev_id;	
	groupid_t				dev_groups[MAX_DEV_GROUPS];
	int					num_groups;
	unsigned int				flags;
	void *					dev_handle;
	int					ver_no;
	time_t					start_time;
	int					remain_old;
	int					remain_mid;
	int					remain_new;
	int					done;
	float					delta;
	int					obj_total;
	float					cur_credits;	/* credits for current iteration */
	int					credit_incr;	/* incremental credits to add */
	int					serviced;	/* times data removed */
	struct search_state *	sc;
} device_handle_t;


#define	MAX_DEV_PER_GROUP	64
typedef struct gid_map {
	struct gid_map *	next;
	groupid_t		gid;
	int			num_dev;
	uint32_t		devs[MAX_DEV_PER_GROUP];
} gid_map_t;


typedef enum {
	SS_ACTIVE,		/* a search is currently in progress */
	SS_DONE,		/* search active, all object are processed */
	SS_EMPTY,
	SS_SHUTDOWN,	
	SS_IDLE	
} search_status_t;


typedef enum {
		CREDIT_POLICY_STATIC = 0,
		CREDIT_POLICY_RAIL,
		CREDIT_POLICY_PROP_TOTAL,
		CREDIT_POLICY_PROP_DELTA

} credit_policies_t;

#define	BG_DEFAULT_CREDIT_POLICY	(CREDIT_POLICY_STATIC)
/*
 * This defines the structures that keeps track of the current search
 * context.  This is the internal state that is kept for consistency,
 * etc.
 */

#define	OBJ_QUEUE_SIZE		1024
struct filter_info;
#ifdef	XXX
typedef struct search_context {
	int			cur_search_id;	/* ID of current search */
	device_handle_t *	dev_list;
	device_handle_t *	last_dev;
	search_status_t		cur_status;	/* current status of search */
	ring_data_t *		proc_ring;	/* processed objects */
	ring_data_t *		bg_ops;	/* unprocessed objects */
	ring_data_t *		log_ring;	/* data to log */
	unsigned long		bg_status;
	int					bg_credit_policy;
	struct filter_data  *	bg_fdata; /* filter_data_t  */
	int			pend_hw;	/* pending hw mark */
	int			pend_lw;	/* pending lw mark */
	void *			dctl_cookie;	/* cookie for dctl library */
	void *			log_cookie;	/* cookie for log library */
} search_context_t;
#endif

#endif