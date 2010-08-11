/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _LIB_SEARCHLET_H_
#define	_LIB_SEARCHLET_H_

/*!  
 * \defgroup searchlet  Searchlet API
 * The searchlet API is the programming interface between OpenDiamond and
 * the Diamond applications running on the host.   All communications with
 * OpenDiamond are performed through this interface.  The programming interface
 * is defined in lib_searchlet.h.
 *
 * */



/*!
 * \file lib_searchlet.h
 * \ingroup searchlet
 * This defines the API that applications use to talk to the rest
 * of the OpenDiamond system.
 */

#include "diamond_consts.h"
#include "diamond_types.h"


#ifdef __cplusplus
extern "C"
{
#endif



/*
 * The following are the functions that make up the searchlet API.
 */


/*!
 * This function is called to initialize search state.  This returns
 * a handle that is used for all other calls. 
 *
 * \return NULL
 *   		The init failed
 *
 * \return non-NULL
 *		A handle that can be used in subsequent calls.
 */

diamond_public
ls_search_handle_t ls_init_search(void);

/*!
 * This function is called to terminate a search and clean up
 * any state associated with the search.  After this call completes
 * the search handle is released and is no-longer valid.  
 *
 * \param handle
 *		the search handle to close.
 *
 * \return 0
 *		the call succeeded and the handle is no longer valid.
 *
 * \return EINVAL
 *		the handle was not valid.
 */

diamond_public
int ls_terminate_search(ls_search_handle_t handle);

/*!
 * This function is called to terminate a search and clean up
 * any state associated with the search.  After this call completes
 * the search handle is released and is no-longer valid.  
 *
 * \param handle
 *		the search handle to close.
 *
 * \param app_stats
 *		pointer to an structure holding application statistics
 *
 * \return 0
 *		the call succeeded and the handle is no longer valid.
 *
 * \return EINVAL
 *		the handle was not valid.
 */

diamond_public
int ls_terminate_search_extended(ls_search_handle_t handle, app_stats_t *as);

/*!
 * This determines the set of objects we are going to search.  This takes
 * a scope definition which specifies the set of objects we are going to
 * search.
 *
 * \param handle
 *		the search handle returned by ls_init_search().
 *
 * \param megacookie
 * 		one or more (concatenated) opendiamond scope cookies
 *
 * \return 0
 *		the call was successful.
 *
 * \return EBUSY
 *		a search is currently active.
 *
 * \return EINVAL
 *		the handle or the cookie was not valid.
 *
 */

diamond_public
int ls_set_scope(ls_search_handle_t handle, const char *megacookie);


/*!
 * This call sets the searchlet for this instance of the search. This
 * can only be called when a search is not active.  If this is called more
 * than once the latest call overwrites the searchlet that was previously 
 * called.  This sets the searchlet for all of the storage devices that
 * contain the group of objects being searched.  If the application wishes
 * to run different searchlets on the devices then they should use
 * ls_set_device_searchlet().
 *
 *
 * \param	handle
 *		The handle for the search instance.
 *
 * \param isa_type
 *		this is the instruction set used in the filter
 * 		file.  If the devices are non-homogeneous, then
 *              the application will need to call this once for
 *              every device type.
 *
 * \param filter_file_name
 *		this is the name of the file where the filter
 * 		files are stored.  This should be a shared library
 * 		file where each filters is a different function as
 * 		specified in the filter specification
 *
 * \param filter_spec_name
 *		The name of the filter specification file.  This is
 * 		read by system to parse the library file as well
 * 		as describing attributes of each of the filters
 * 		(dependencies, ...).
 *
 * \return 0
 * 		The searchlet was set appropriately.
 *
 * \return EINVAL
 *		One of the file names was invalid or one of the files 
 *		could not be parsed.
 *
 * \return EBUSY
 *		A search was already active.
 */

diamond_public
int ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
                     char *filter_file_name, char *filter_spec_name);


/*!
 * This call adds another filter file to the searchlet.  This can only
 * be called after one successful call to ls_set_searchlet().
 *
 * \param	handle
 *		The handle for the search instance.
 *
 * \param isa_type
 *		this is the instruction set used in the filter
 * 		file.  If the devices are non-homogeneous, then
 * 		the application will need to call this once for
 * 		every device type.
 * 
 * \param	filter_file_name
 *		this is the name of the file where the filter
 * 		files are stored.  This should be a shared library
 * 		file where each filters is a different function as
 * 		 specified in the filter specification
 *
 * \return 0
 * 		The searchlet was set appropriately.
 *
 * \return EINVAL
 *		One of the file names was invalid or one of the files 
 *		could not be parsed.
 *
 * \return EBUSY
 *		A search was already active.
 */
diamond_public
int ls_add_filter_file(ls_search_handle_t handle, device_isa_t isa_type,
                     char *filter_file_name);


/*!
 * This call sets a "blob" of data to be passed to a given
 * filter.  This is a way to pass a large amount of data.
 *
 * This call should be called after the searchlet has been
 * loaded but before a search has begun.
 *
 * NOTE:  It is up to the caller to make sure this data
 * can be interpreted by at the device (endian issues, etc).
 *
 * \param handle 
 * 		The handle for the search instance.
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
 *
 * \return 0
 *		The call succeeded.
 *
 * \return EINVAL
 *		One of the file names was invalid or 
 * 	   	one of the files could not be parsed.
 *
 * \return EBUSY
 * 		A search was already active.
 */

diamond_public
int ls_set_blob(ls_search_handle_t handle, char *filter_name,
                int  blob_len, void *blob_data);


/*!
 * This call sets the list of pushed, or thumbnail, attributes for which data
 * will be returned to the client over the blast channel when the searchlet has
 * not dropped the object. The pushed attribute set will implicitly include the
 * "_ObjectID" attribute as it is needed for filter reexecution.
 *
 * See also ls_reexecute_filters()
 *
 * \param handle
 *		The search handle returned by ls_init_search().
 *
 * \param attributes
 *		A NULL terminated array of attribute names.
 *
 * \return 0
 *		The call succeeded.
 *
 * \return EINVAL
 *		If the handle is not valid.
 */
diamond_public
int ls_set_push_attributes(ls_search_handle_t handle, const char **attributes);


/*!
 * This call begins the execution of a searchlet.  For this call to 
 * succeed there must be an active searchlet that was set by lib_set_searchlet()
 *
 * \param handle
 * 		the search handle returned by init_libsearchlet().
 *
 * \return 0
 *		If the search started correctly.
 *
 * \return EINVAL
 *		If the searchlet hasn't been initialized or if
 * 		initialization calls have not been made, or if
 * 		the handle is not valid.
 */
diamond_public
int ls_start_search(ls_search_handle_t handle);


/*!
 * This call gets the next object that matches the searchlet.  The flags specify
 * the behavior for blocking.  If no flags are passed, then the call will block
 * until the next object is available or the search has completed.  If the flag
 * LSEARCH_NO_BLOCK is set, and no objects are currently available, then
 * the error EWOULDBLOCK is returned.
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().
 *
 * \param obj_handle
 *		a pointer to the location where the new object handle will
 *		stored upon successful completion of the call.
 *
 * \param flags
 *		Flags to control the behavior.
 *
 * \return 0
 *		Successful, obj_handle contains pointer to new object.
 *
 * \return EINVAL
 *		There was no active search or the handle is invalid.
 *
 * \return EWOULDBLOCK
 *		There are no objects currently available.
 *
 * \return ENOENT
 *		The search has been completed and all objects have
 *		been searched.
 *
 */

diamond_public
int ls_next_object(ls_search_handle_t handle,
                   ls_obj_handle_t *obj_handle,
                   int flags);


/*!
 * Flags  for ls_next_object()
 */
#define	LSEARCH_NO_BLOCK		0x01 	/** Do not block */

/*!
 * This call obtains a unique identifier which can be used in conjuction with
 * ls_reexecute_filters to reexecute filters and get back a new object handle.
 *
 * \param handle
 *		The search handle returned by ls_init_search().
 *
 * \param obj_handle
 *		A handle to the object for which we want a unique identifier.
 *
 * \param objectid
 *		Will be set to an allocated string that contains an unique
 *		object identifier which can be passed to ls_reexecute_filters.
 *
 * \return 0
 *		Returns an allocated string that contains a unique object
 *		identifier which can be passed to ls_reexecute_filters.
 *
 * \return EINVAL
 *		If we are unable to get a valid objectid for the object.
 *
 * \return ENODEV
 *		If the device that sent the object could not be found.
 */
diamond_public
int ls_get_objectid(ls_search_handle_t handle, ls_obj_handle_t obj_handle,
		    const char **objectid);

/*!
 * This call forces searchlet reexecution for the given object and retrieves
 * attribute data for the specified attributes.
 *
 * \param handle
 *		The search handle returned by ls_init_search().
 *
 * \param objectid
 *		An unique object identifier as returned by a previous call to
 *		ls_get_objectid().
 *
 * \param attributes
 *		A NULL terminated array of attribute names. If the array is
 *		NULL, the returned object will be populated with all
 *		attributes.
 *
 * \param obj_handle
 *		A pointer to the location where the new object handle will
 *		stored upon successful completion of the call.
 *
 * \return 0
 *		Successful, obj_handle contains pointer to a new object.
 *
 * \return EINVAL
 *		If the objectid is invalid.
 *
 * \return ENODEV
 *		If the device that sent the object could not be found.
 *
 * \return ENOTCONN
 *		If the device that sent the object is no longer available.
 */
diamond_public
int ls_reexecute_filters(ls_search_handle_t handle,
			 const char *objectid, const char **attributes,
			 ls_obj_handle_t *obj_handle);


/*!
 * This call is performed by the application to release object it obtained 
 * through ls_next_object.  This will causes all object storage and 
 * associated  state to be freed.  It will also invalidate any data to
 * the object obtained through the filter API.
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().
 *
 * \param	obj_handle
 * 		The handle for the object to release.
 *
 * \return 0
 *		The release was successfully.
 *
 * \return EINVAL
 *		Either the object or the search handle was invalid.
 *
 */

diamond_public
int ls_release_object(ls_search_handle_t handle,
                      ls_obj_handle_t obj_handle);


/*
 * 
 * These are the device management calls.
 */


/*!
 * This gets a list of all the storage devices that will be involved in
 * the search.  The results are returned as an array of device handles.   
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().

 * \param handle_list
 *		A pointer to a caller allocated array of device handles.
 *
 * \param num_handles
 *		A pointer to an integer.  The caller sets this value to
 *       	indicate the space allocated in handle list.  On return,
 * 		this value will hold the number of handles filled in.  
 *		If the caller did not allocate sufficient space, then 
 *		ENOSPC will be returned and the num_handles will 
 *		indicate the space necessary for the call to succeed.
 *
 * \return 0
 *		the call was successful.
 *
 * \return EBUSY
 * 		a search is currently active.
 *
 * \return ENOSPC
 * 		The caller did not provide enough storage (the value 
 *       	stored at num_handles was too small).  In this case the 
 * 		value stored at num_handles will be updated to 
 * 		indicate the amount of space needed.
 *
 */
diamond_public
int ls_get_dev_list(ls_search_handle_t handle,
                    ls_dev_handle_t *handle_list,
                    int *num_handles);

/*!
 * This call takes a specific device handle and returns the characteristics of
 * this device.  
 *
 * \param	handle
 *		the search handle returned by init_libsearchlet().
 *
 * \param dev_handle
 *		the handle for the device being queried.
 *
 * \param dev_chars
 *		A pointer to the location where the device 
 *		characteristics should be stored.
 *
 * \return 0
 *		Call succeeded.
 *
 * \return EINVAL
 *		One of the handles is not valid.
 *
 */

diamond_public
int ls_dev_characteristics(ls_search_handle_t handle,
                           ls_dev_handle_t dev_handle,
                           device_char_t *dev_chars);

diamond_public
const char *ls_get_dev_name(ls_search_handle_t handle,
			    ls_dev_handle_t dev_handle);

/*!
 * This call gets the current statistics from device specified by the device
 * handle.  This includes statistics on the device as well as any currently 
 * running search.
 * 
 * \param handle
 *		The handle for the search instance.
 *
 * \param dev_handle
 *		The handle for the device being queried.
 *
 * \param dev_stats
 *		This is the location where the device statistics should
 *		be stored.  This is allocated by the caller.
 * 
 * \param stat_len
 *		A pointer to an integer.  The caller sets this value to
 *		the amount of space allocated for the statistics.  Upon
 *		return, the call will set this to the amount of 
 *		space used.  If the call failed because of 
 *		insufficient space, ENOSPC, the call the will set 
 *		this value to the amount of space needed.
 *
 * \return 0
 *		The call completed successfully.
 *
 * \return ENOSPC
 *		The caller did not allocated sufficient space for 
 * 	 	the results.
 *
 * \return EINVAL
 *		Either the search handle or the device handle are invalid.
 *
 */

diamond_public
int ls_get_dev_stats(ls_search_handle_t handle,
                     ls_dev_handle_t  dev_handle,
                     dev_stats_t *dev_stats, int *stat_len);


/*!
 * This call gets total number of objects in the current search.  Because
 * connections are setup asynchronously, there is a time lag between when
 * a search is started and when this reaches its true value.
 * 
 * \param handle
 *		The handle for the search instance.
 *
 * \param obj_cnt
 *		Pointer to integer where the value will be returned.
 *
 * \return 0
 *		The call completed successfully.
 *
 * \return EINVAL
 *		Either the search handle is invalid.
 */

diamond_public
int
ls_num_objects(ls_search_handle_t handle, int *obj_cnt);



diamond_public
int
ls_get_dev_session_variables(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
			     device_session_vars_t **vars);

diamond_public
int
ls_set_dev_session_variables(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
			     device_session_vars_t *vars);


/*!
 * This call advises Diamond of the user's state.  
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().
 *
 * \return 0
 *		The state was set successfully.
 *
 * \return EINVAL
 *		There was no active search or the handle is invalid.
 */

diamond_public
int ls_set_user_state(ls_search_handle_t handle, user_state_t state);


/*!
 * Get pointer to first attribute and it's data.  
 * \param ohandle
 * 		the object handle.
 *
 * \param name
 *		Pointer where name pointer will be stored 
 *
 * \param len
 *		A pointer to the location where the length
 * 		attribute data will be stored.
 *
 * \param data
 *		A pointer to where the data pointer will be stored.
 *
 * \return 0
 *		Attributes were read successfully.
 *
 * \return EINVAL
 *		One or more of the arguments was invalid.
 */

diamond_public
int ls_first_attr(ls_obj_handle_t ohandle, char **name, 
		  size_t *len, unsigned char **data, void **cookie);

/*!
 * Get pointer to first attribute and it's data.  
 * \param ohandle
 * 		the object handle.
 *
 * \param name
 *		Pointer where name pointer will be stored 
 *
 * \param len
 *		A pointer to the location where the length
 * 		attribute data will be stored.
 *
 * \param data
 *		A pointer to where the data pointer will be stored.
 *
 * \return 0
 *		Attributes were read successfully.
 *
 * \return EINVAL
 *		One or more of the arguments was invalid.
 */

diamond_public
int ls_next_attr(ls_obj_handle_t ohandle, char **name, 
		 size_t *len, unsigned char **data, void **cookie);



/*!
 * This function allows the programmer to log some data that
 * can be retrieved from the host system.
 *
 * \param level
 *		The log level associated with the command.  This
 * 		used to limit the amount of information being passed.
 *
 * \param fmt
 *		format string used for parsing the data.  This uses
 * 		printf syntax
 *
 * \param ...
 *		the arguments for the format.
 *
 */

diamond_public
void ls_log(int level, const char *fmt, ...);


/*!
 * Read an attribute from the object into the buffer space provided
 * by the caller.  This does invoke a copy and for large structures
 * it is preferable to use ls_ref_attr() if the caller will not
 * be modifying the attribute.
 *
 * \param ohandle
 * 		the object handle.
 *
 * \param name
 *		The name of the attribute to read.
 *
 * \param len
 *		A pointer to the location where the length
 * 		of the data storage is stored.  The caller
 * 		sets this to the amount of space allocated.
 *		Upon return this is set to the size of
 * 		data actually read.  If there is not enough
 * 		space then ENOSPC is returned and the value is
 * 		updated to the size needed to successfully complete
 * 		the call.
 *
 * \param data
 *		The location where the results should be stored.
 *
 * \return 0
 *		Attributes were read successfully.
 *
 * \return EINVAL
 *		One or more of the arguments was invalid.
 */

diamond_public
int ls_read_attr(ls_obj_handle_t ohandle, const char *name, size_t *len,
		 unsigned char *data);

/*!
 * Get pointer to attribute data in an object.  The returned pointer should
 * be treated read-only, and is only valid in the current instance of the
 * filter.  
 * \param ohandle
 * 		the object handle.
 *
 * \param name
 *		The name of the attribute to read.
 *
 * \param len
 *		A pointer to the location where the length
 * 		attribute data will be stored.
 *
 * \param data
 *		A pointer to where the data pointer will be stored.
 *
 * \return 0
 *		Attributes were read successfully.
 *
 * \return ENOSPC
 *		The provided buffer was not large enough to hold
 *
 * \return EINVAL
 *		One or more of the arguments was invalid.
 */

diamond_public
int ls_ref_attr(ls_obj_handle_t ohandle, const char *name,
		size_t *len, unsigned char **data);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_SEARCHLET_H_  */


