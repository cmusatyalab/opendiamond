#ifndef _LIB_SEARCHLET_H_
#define	_LIB_SEARCHLET_H_ 

#include "consts.h"
#include "rtimer.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Some of the common data structures used by the library calls
 */


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


typedef uint64_t	group_id_t;


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
	uint64_t	dc_speed;	/* CPU speed, (some bogomips, etc.) */
	uint64_t	dc_mem;		/* Available memory for the system  */
} device_char_t;

/*
 * The following are the functions that make up the searchlet API.
 */


/*
 * This function is called to initialize search state.  This returns
 * a handle that is used for all other calls. 
 *
 * Args:
 * 	none
 *
 * Returns:
 * 	NULL	 - the library failed to initialize.
 * 	non-NULL - a handle that can be used for subsequent calls.
 */

extern ls_search_handle_t ls_init_search();

/*
 * This function is called to terminate a search and clean up
 * any state associated with the search.  After this call completes
 * the search handle is released and is no-longer valid.  
 *
 * Args:
 * 	handle	  - the search handle to close.
 *
 * Returns:
 * 	0	  - the call succeeded and the handle is no longer valid.
 * 	EINVAL	  - the handle was not valid.
 */

extern int ls_terminate_search(ls_search_handle_t handle);


/*
 * This determines the set of objects we are going to search.  This takes
 * a list of group ID's that contain the set of objects we are going
 * to search.
 *
 * Args:
 * 	handle	- the search handle returned by init_libsearchlet().
 *	
 *	num_groups - the number of groups in the list.
 *
 *	glist   - An array of groups with num groups elements.
 *
 * Returns:
 * 	0  	- the call was successful.
 * 	EBUSY   - a search is currently active.
 * 	EINVAL  - the handle or the search list is not valid.
 */

extern int ls_set_searchlist(ls_search_handle_t, int num_groups, 
		group_id_t *glist);


/*
 * This call sets the searchlet for this instance of the search. This
 * can only be called when a search is not active.  If this is called more
 * than once the latest call overwrites the searchlet that was previously 
 * called.  This sets the searchlet for all of the storage devices that
 * contain the group of objects being searched.  If the application wishes
 * to run different searchlets on the devices then they should use
 * ls_set_device_searchlet().
 *
 *
 * Args:
 * 	handle           - The handle for the search instance.
 *
 * 	isa_type         - this is the instruction set used in the filter
 * 			   file.  If the devices are non-homogeneous, then
 * 		           the application will need to call this once for
 *                         every device type.
 *
 * 	filter_file_name - this is the name of the file where the filter
 * 			   files are stored.  This should be a shared library
 * 			   file where each filters is a different function as
 * 			   specified in the filter specification
 *
 * 	filter_spec_name - The name of the filter specification file.  This is
 * 			   read by system to parse the library file as well
 * 			   as describing attributes of each of the filters
 * 			   (dependencies, ...).
 *
 * Returns:
 * 	0                - The searchlet was set appropriately.
 *
 * 	EINVAL           - One of the file names was invalid or 
 * 	                   one of the files could not be parsed.
 *
 *	EBUSY		 - A search was already active.
 */

extern int ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
		            char *filter_file_name, char *filter_spec_name);


/*
 * This call sets the searchlet for for a specific device.  This call can
 * be used if the application wishes to set filters for different devices.
 * In this case the appliatoin must make this call for each of the devices
 * involved in the search.
 *
 * Args:
 * 	handle           - The handle for the search instance.
 *
 *	dev_handle	 - The handle for the device.
 *
 * 	isa_type         - this is the instruction set used in the filter
 * 			   file.  If the devices are non-homogeneous, then
 * 		           the application will need to call this once for
 *                         every device type.
 *
 * 	filter_file_name - this is the name of the file where the filter
 * 			   files are stored.  This should be a shared library
 * 			   file where each filters is a different function as
 * 			   specified in the filter specification
 *
 * 	filter_spec_name - The name of the filter specification file.  This is
 * 			   read by system to parse the library file as well
 * 			   as describing attributes of each of the filters
 * 			   (dependencies, ...).
 *
 * Returns:
 * 	0                - The searchlet was set appropriately.
 *
 * 	EINVAL           - One of the file names was invalid or 
 * 	                   one of the files could not be parsed.
 *
 *	EBUSY		 - A search was already active.
 */

extern int ls_set_device_searchlet(ls_search_handle_t handle, 
				   ls_dev_handle_t dev_handle,
				   device_isa_t isa_type,
		                   char *filter_file_name, 
			           char *filter_spec_name);



/*
 * This call begins the execution of a searchlet.  For this call to 
 * succeed there must be an active searchlet that was set by lib_set_searchlet()
 *
 * Args:
 * 	handle - the search handle returned by init_libsearchlet().
 *
 * Returns:
 * 	0           -	If the search started correctly.
 *
 * 	EINVAL      -	If the searchlet hasn't been initialized or if
 * 			initialization calls have not been made, or if
 * 			the handle is not valid.
 *
 * XXX:  Do we need this, do we just start when the searchlet is set?
 */
extern int ls_start_search(ls_search_handle_t handle);


/*
 * This call terminates a currently running search.  When the call returns
 * the search has been terminated from the applications perspective and the
 * application is able to change the searchlet, etc. if it wishes.
 *
 * Args:
 * 	handle	- the search handle returned by init_libsearchlet().
 *
 * Return:
 * 	0          - The search aborted cleanly.
 *
 * 	EINVAL     - There was no active search or the handle is invalid.
 */

extern int ls_abort_search(ls_search_handle_t handle);


/*
 * This call gets the next object that matches the searchlet.  The flags specify
 * the behavior for blocking.  If no flags are passed, then the call will block
 * until the next object is available or the search has completed.  If the flag
 * LSEARCH_NO_BLOCK is set, and no objects are currently available, then
 * the error EWOULDBLOCK is returned.
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	obj_handle - a pointer to the location where the new object handle will
 * 	             stored upon succesful completion of the call.
 *
 * Return:
 * 	0          - The search aborted cleanly.
 *
 * 	EINVAL     - There was no active search or the handle is invalid.
 *
 *	EWOULDBLOCK - There are no objects currently available.
 *
 *	ENOENT	   - The search has been completed and all objects have
 *		     been searched.
 *
 */

#define	LSEARCH_NO_BLOCK		0x01
extern int ls_next_object(ls_search_handle_t handle, 
			       ls_obj_handle_t *obj_handle,
		               int flags);

/*
 * These calls allow us to manipulate an object returned through ls_next_obj.
 * These is an open question if these should be part of this library or
 * if we should use a generic object disk library to perform these calls.
 * These are a placeholder for now. 
 */
 
/*
 * This call is performed by the application to release object it obtained 
 * through ls_next_object.  This will causes all object storage and 
 * assocaited  state to be freed.  It will also invalidate all object 
 * mappings obtained through ls_map_object().
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	obj_handle - the object handle.
 *
 * Return:
 * 	0	   - the search aborted cleanly.
 *
 * 	EINVAL     - one of the handles was invalid. 
 */

extern int ls_release_object(ls_search_handle_t handle, 
		             ls_obj_handle_t obj_handle);


/*
 * This reads a range of the objects and returns a pointer where to a
 * memory location that holds the data.
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	obj_handle - the object handle.
 *
 * 	start	   - the starting offset into the object.
 *
 * 	len	   - This is a pointer to the number of bytes to read.  On
 *  	     	     return this location will hold the number of bytes that
 *		     were actually read.
 *
 * 	bufp	   - a pointer to the location where the buffer pointer
 * 		     will be stored.
 *
 * Return:
 * 	0	   - the read was successful. 
 *
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 * 
 * 	EINVAL     - one of the handles was invalid. 
 *
 */
extern int ls_read_object(ls_search_handle_t handle, ls_obj_handle_t obj_handle,
	       		 off_t  start, off_t *len, char **bufp);

/*
 * This reads a writes to a range of the object.  On this call
 * the library takes possesion of the buffer, the caller should not
 * access memory owned by the buffer or free the buffer after we are done. 
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	obj_handle - the object handle.
 *
 * 	start	   - the starting offset into the object.
 *
 * 	len	   - the number of bytes to write.
 *
 * 	buf	   - The buffer to write.
 *
 * Return:
 * 	0	   - the write was successful. 
 *
 * 	EINVAL     - one of the handles was invalid. 
 */
extern int ls_write_object(ls_search_handle_t handle, 
			   ls_obj_handle_t obj_handle,
	       		   off_t start, off_t len, char *buf);


/*
 * This allocated an empty buffer for the application to use.
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	len	   - the number of bytes to write.
 *
 * 	bufp	   - The pointer where the buffer will be stored.
 *
 * Return:
 * 	0	   - the write was successful. 
 * 
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 *
 * 	EINVAL     - one of the handles was invalid. 
 */
extern int ls_alloc_buffer(ls_search_handle_t handle, off_t len, char **buf);

/*
 * This frees a buffer that was allocated through a read call or
 * or ls_alloc_buffer().
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	buf	   - The buffer to release.
 *
 * Return:
 * 	0	   - the write was successful. 
 * 
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 *
 * 	EINVAL     - one of the handles was invalid. 
 */
extern int ls_free_buffer(ls_search_handle_t handle, char *buf);


/*
 * This creates a new temporary object for use by the applicatoin.  This
 * object is only valid until it is released or the search is terminated.
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	obj_handle - A pointer where the new object handle will be stored.
 *
 * Return:
 * 	0	   - the write was successful. 
 * 
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 *
 * 	EINVAL     - one of the handles was invalid. 
 */
extern int ls_create_object(ls_search_handle_t handle, 
			    ls_obj_handle_t *obj_handle);


/*
 * This a copy of an existing object.  This will copy all of the attributes,
 * etc. as well as the data.
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	old_obj    - the object to copy.
 * 
 *      new_obj    - a pointer where the new object handle will be stored.
 *
 * Return:
 * 	0	   - the write was successful. 
 * 
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 *
 * 	EINVAL     - one of the handles was invalid. 
 */
extern int ls_copy_object(ls_search_handle_t handle,  ls_obj_handle_t old_obj,
			    ls_obj_handle_t *new_obj);


/*
 * 
 * These are the device management calls.
 */


/*
 * This gets a list of all the storage devices that will be involved in
 * the search.  The results are returned as an array of device handles.   
 *
 * Args:
 * 	handle	    - the search handle returned by init_libsearchlet().
 *
 * 	handle_list - A pointer to a caller allocated array of device handles.
 *
 * 	num_handles - A pointer to an integer.  The caller sets this value to
 * 		      indicate the space allocated in handle list.  On return,
 * 		      this value will hold the number of handles filled in.  
 * 		      If thecaller did not allocate sufficient space, then 
 * 		      ENOSPC will be returned and the num_handles will 
 * 		      indicate the space necessary for the call to succeed.
 *
 * Returns:
 * 	0  	    - the call was successful.
 *
 * 	EBUSY       - a search is currently active.
 *
 * 	ENOSPC      - The caller did not provide enough storage (the value 
 * 		      stored at num_handles was too small).  In this case the 
 * 		      value stored at num_handles will be updated to 
 * 		      indicate the amount of space needed.
 *
 */
extern int ls_get_dev_list(ls_search_handle_t handle, 
		            ls_dev_handle_t *handle_list,
			    int *num_handles);

/*
 * This call takes a specific device handle and returns the characteristics of
 * this device.  
 *
 * Args:
 * 	handle	    - the search handle returned by init_libsearchlet().
 *
 * 	dev_handle  - The handle for the device being queried.
 *
 * 	dev_char    - A pointer to the location where the device 
 * 		      charactersitics should be stored.
 *
 * Returns:
 *	0	    - Call succeeded.
 *
 *	EINVAL      -  One of the handles is not valid.
 * 	
 *
 */

extern int ls_dev_characteristics(ls_search_handle_t handle, 
			          ls_dev_handle_t dev_handle,
			          device_char_t *dev_chars);

/*
 * This call gets the current statistics from device specified by the device
 * handle.  This includes statistics on the device as well as any currently 
 * running search.
 * 
 * Args:
 * 	handle         - The handle for the search instance.
 *
 * 	dev_handle     - The handle for the device being queried.
 *
 * 	dev_stats      - This is the location where the device statistics should
 * 			 be stored.  This is allocated by the caller.
 * 
 * 	stat_len       - A pointer to an integer.  The caller sets this value to
 * 			 the amount of space allocated for the statistics.  Upon
 * 			 return, the call will set this to the amount of 
 * 			 space used.  If the call failed because of 
 * 			 insufficient space, ENOSPC, the call the will set 
 * 			 this value to the amount of space needed.
 *
 * Returns:
 * 	0              - The call completed successfully.
 *
 * 	ENOSPC	       - The caller did not allocated sufficient space for 
 * 			 the results.
 *
 * 	EINVAL	       - Either the search handle or the device handle are 
 * 			 invalid.
 *
 */

extern int ls_get_dev_stats(ls_search_handle_t handle, 
		 	     ls_dev_handle_t  dev_handle,
		             dev_stats_t *dev_stats, int *stat_len);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_SEARCHLET_H_  */


