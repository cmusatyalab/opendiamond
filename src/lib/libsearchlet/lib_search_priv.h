/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _LIB_SEARCH_PRIV_H_
#define _LIB_SEARCH_PRIV_H_

#include "ring.h"
#include "odisk_priv.h"

/*
 * This is header file defines the internal state that is used
 * for the searchlet library.
 */


/* Some constants */
#define	LS_OBJ_PEND_LW	50

/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */
#define	DEV_FLAG_RUNNING		0x01
#define	DEV_FLAG_COMPLETE		0x02
#define	DEV_FLAG_BLOCKED		0x04
#define	DEV_FLAG_DOWN			0x08

struct search_context;

#define	MAX_DEV_GROUPS		64

typedef struct device_handle {
	struct device_handle * 		next;
	char *				dev_name;
	groupid_t			dev_groups[MAX_DEV_GROUPS];
	int				num_groups;
	unsigned int			flags;
	void *				dev_handle;
	int				ver_no;
	time_t				start_time;
	int				remain_old;
	int				remain_mid;
	int				remain_new;
	float				done;
	float				prate;
	int				obj_total;
	uint32_t			serviced;	/* times data removed */
	struct 				search_context *	sc;
} device_handle_t;


typedef enum {
    SS_ACTIVE,		/* a search is currently in progress */
    SS_DONE,		/* search active, all object are processed */
    SS_EMPTY,
    SS_SHUTDOWN,
    SS_IDLE
} search_status_t;

/*
 * This defines the structures that keeps track of the current search
 * context.  This is the internal state that is kept for consistency,
 * etc.
 */

#define	OBJ_QUEUE_SIZE		1024
struct filter_info;
typedef struct search_context {
	double			avg_proc_time;	/* time spent per object */
	device_handle_t *	dev_list;
	device_handle_t *	last_dev;
	search_status_t		cur_status;	/* current status of search */
	ring_data_t *		proc_ring;	/* processed objects */
	ring_data_t *		bg_ops;		/* unprocessed objects */
	ring_data_t *		log_ring;	/* data to log */
	unsigned long		bg_status;
	struct filter_data *	bg_fdata; 	/* filter_data_t  */
	uint32_t		pend_lw;	/* pending lw mark */
	int 		search_exec_mode;  /* a search_mode_t */
	host_stats_t	host_stats;		/* object stats for this search */
} search_context_t;

/*
 * These are the prototypes of the device operations that
 * in the file ls_device.c
 */
int dev_new_obj_cb(void *hcookie, obj_data_t *odata, int vno);
void dev_log_data_cb(void *cookie, char *data, int len, int devid);
int lookup_group_hosts(groupid_t gid, int *num_hosts, char *hosts[]);
int device_add_gid(search_context_t *sc, groupid_t gid, const char *host);
/*
 * These are background processing functions.
 */
int bg_init(search_context_t *sc);
int bg_set_lib(search_context_t *sc, sig_val_t *sig);
int bg_set_spec(search_context_t *sc, sig_val_t *sig);
int bg_set_blob(search_context_t *sc, char *filter_name,
		int blob_len, void *blob_data);
int bg_start_search(search_context_t *sc);
int bg_stop_search(search_context_t *sc);


int log_start(search_context_t *sc);


int dctl_start(search_context_t *sc);


/*!
 * This call sets a "blob" of data to be passed to a given
 * filter on a specific device.  This is similar to the above
 * call but will only affect one device instead of all devices.
 *
 * This call should be called after the searchlet has been
 * loaded but before a search has begun.
 *
 * NOTE:  It is up to the caller to make sure the data
 * can be interpreted by at the device (endian issues, etc).
 *
 * \param handle
 *		The handle for the search instance.
 *
 * \param dev_handle
 *		The handle for the device.
 *
 * \param filter_name
 *		The name of the filter to use for the blob.
 *
 * \param blob_len
 *		The length of the blob data.
 *
 * \param blob_data
 *		A pointer to the blob data.
 * 
 * \return 0
 *		The call succeeded.
 *
 * \return EINVAL
 *		One of the file names was invalid or 
 *		one of the files could not be parsed.
 *
 * \return EBUSY
 *		A search was already active.
 */

int ls_set_device_blob(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
                       char *filter_name, int  blob_len, void *blob_data);




/*!
 * This call terminates a currently running search.  When the call returns
 * the search has been terminated from the applications perspective and the
 * application is able to change the searchlet, etc. if it wishes.
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().
 *
 * \return 0
 *		The search aborted cleanly.
 *
 * \return EINVAL
 *		There was no active search or the handle is invalid.
 */

int ls_abort_search(ls_search_handle_t handle);


/*!
 * This call terminates a currently running search.  When the call returns
 * the search has been terminated from the applications perspective and the
 * application is able to change the searchlet, etc. if it wishes.
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().
 *
 * \param app_stats
 *		pointer to an structure holding application statistics
 *
 * \return 0
 *		The search aborted cleanly.
 *
 * \return EINVAL
 *		There was no active search or the handle is invalid.
 */

int ls_abort_search_extended(ls_search_handle_t handle, app_stats_t *as);



/*!
 * This call sets the searchlet for for a specific device.  This call can
 * be used if the application wishes to set filters for different devices.
 * In this case the application must make this call for each of the devices
 * involved in the search.
 *
 * \param	handle
 * 		The handle for the search instance.
 *
 * \param dev_handle
 *		The handle for the device.
 *
 *  
 * \param isa_type
 *		this is the instruction set used in the filter file.  If the 
 * 		devices are non-homogeneous, the application will need to call 
 *  	this once for every device type.
 *
 * \param	filter_file_name
 *		this name of the file that contains the filter code.  This 
 *		should be a shared library file where each filters is a 
 *		different function as specified in the filter specification
 *
 * \param filter_spec_name
 *		The name of the filter specification file.  This is
 * 	 	read by system to parse the library file as well
 * 	 	as describing attributes of each of the filters
 * 	  	(dependencies, ...).
 *
 * \return 0
 *		The searchlet was set appropriately.
 *
 * \return EINVAL
 *		One of the file names was invalid or one of the files 
 *		could not be parsed.
 *
 * \return EBUSY
 *		A search was already active.
 */

int ls_set_device_searchlet(ls_search_handle_t handle,
                            ls_dev_handle_t dev_handle,
                            device_isa_t isa_type, char *filter_file_name,
                            char *filter_spec_name);


/* Determine the set of objects we are going to search. */
int ls_set_searchlist(ls_search_handle_t handle, int num_groups,
                      groupid_t *glist);


#endif
