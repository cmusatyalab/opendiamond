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

#ifndef _LIB_SEARCHLET_H_
#define	_LIB_SEARCHLET_H_ 

#include "rtimer.h"
#include "diamond_consts.h"
#include "diamond_types.h"


#ifdef __cplusplus
extern "C" {
#endif



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

ls_search_handle_t ls_init_search();

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

int ls_terminate_search(ls_search_handle_t handle);


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

int ls_set_searchlist(ls_search_handle_t, int num_groups, 
			     groupid_t *glist);


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

int ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
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

int ls_set_device_searchlet(ls_search_handle_t handle, 
				   ls_dev_handle_t dev_handle,
				   device_isa_t isa_type, char *filter_file_name, 
			           char *filter_spec_name);



/*
 * This call sets a "blob" of data to be passed to a given
 * filter.  This is a way to pass a large amount of data.
 *
 * This call should be called after the searchlet has been
 * loaded but before a search has begun.
 *
 * NOTE:  It is up to the caller to make sure this data
 * can be interpreted by at the device (endian issues, etc).
 *
 * Args:
 * 	handle          -	The handle for the search instance.
 *
 *  filter_name		- 	The name of the filter to use for the blob.
 *
 *  blob_len		- 	The length of the blob data.
 *
 *  blob_data		-	A pointer to the blob data.
 * 
 *
 * Returns:
 * 	0                - The call suceeded.
 *
 * 	EINVAL           - One of the file names was invalid or 
 * 	                   one of the files could not be parsed.
 *
 *	EBUSY		 	 - A search was already active.
 */

int ls_set_blob(ls_search_handle_t handle, char *filter_name,
                   int  blob_len, void *blob_data);


/*
 * This call sets a "blob" of data to be passed to a given
 * filter on a specific device.  This is similiar to the above
 * call but will only affect one device instead of all devices.
 *
 * This call should be called after the searchlet has been
 * loaded but before a search has begun.
 *
 * NOTE:  It is up to the caller to make sure this data
 * can be interpreted by at the device (endian issues, etc).
 *
 * Args:
 * 	handle          -	The handle for the search instance.
 *
 *	dev_handle	 - The handle for the device.
 *
 *  filter_name		- 	The name of the filter to use for the blob.
 *
 *  blob_len		- 	The length of the blob data.
 *
 *  blob_data		-	A pointer to the blob data.
 * 
 *
 * Returns:
 * 	0                - The call suceeded.
 *
 * 	EINVAL           - One of the file names was invalid or 
 * 	                   one of the files could not be parsed.
 *
 *	EBUSY		 	 - A search was already active.
 */

int ls_set_device_blob(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
				char *filter_name, int  blob_len, void *blob_data);


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
int ls_start_search(ls_search_handle_t handle);


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

int ls_abort_search(ls_search_handle_t handle);


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
int ls_next_object(ls_search_handle_t handle, 
			       ls_obj_handle_t *obj_handle,
		               int flags);

/* XXX */
int
ls_num_objects(ls_search_handle_t handle, int *obj_cnt);
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

int ls_release_object(ls_search_handle_t handle, 
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
int ls_read_object(ls_search_handle_t handle, ls_obj_handle_t obj_handle,
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
int ls_write_object(ls_search_handle_t handle, 
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
int ls_alloc_buffer(ls_search_handle_t handle, off_t len, char **buf);

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
int ls_free_buffer(ls_search_handle_t handle, char *buf);


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
int ls_create_object(ls_search_handle_t handle, 
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
int ls_copy_object(ls_search_handle_t handle,  ls_obj_handle_t old_obj,
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
 int ls_get_dev_list(ls_search_handle_t handle, 
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

int ls_dev_characteristics(ls_search_handle_t handle, 
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

int ls_get_dev_stats(ls_search_handle_t handle, 
		 	     ls_dev_handle_t  dev_handle,
		             dev_stats_t *dev_stats, int *stat_len);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_SEARCHLET_H_  */


