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


/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#ifndef _LIB_FILTER_H_
#define	_LIB_FILTER_H_

/*!  \defgroup Filter  Filter API
 * The filter API is the API used by the searchlets to interact with
 * the Diamond run-time when evaluating objects.  It provides interfaces
 * to read/write object data as well as reading and writing object
 * attributes.  This API can also be used by the host application
 * to manipulate data objects.  The programming interface is defined
 * in lib_filter.h
 *  */

/*!
 * \file lib_filter.h
 * \ingroup filter
 * The API for the functions that are available to filters running on
 * diamond system.
 */



#include <sys/types.h>		/* for size_t */
#include "lib_log.h"		/* for log levels */

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * A handle to an object.
 */
typedef	void *	lf_obj_handle_t;


/*!
 * Prototype for the filter init function.
 *
 * This function takes a list of filter arguments as well as
 * a blob of data.  The init function then allocates any data structures
 * and sets filter_args to point to the allocated data.
 *
 * The init function should return 0 if it initialized correction.  Any
 * other value is treated as a fatal condition.
 *
 * \param num_arg
 *		number of arguments
 *
 * \param args
 *		An array of num_arg arguments
 *
 * \param bloblen
 * 		The length of a blob of memory from the application program.
 *
 * \param blob_data
 *		An opaque blob of data from the application.
 *
 * \param filt_name
 *		The name of the filter being initialized.
 *
 * \param filter_args
 *		Location to store a pointer to the filters private data.
 * 
 * \return 0
 *		Initialization was successful.
 *
 * \return Non-zero
 *		Fatal condition
 */

typedef int (*filter_init_proto)(int num_arg, char **args, int bloblen,
				 void *blob_data, const char * filt_name,
				 void **filter_args);


/*!
 * This is the prototype for a filter evaluation function.
 *
 * The return value is an integer that represents a confidence.  As
 * part of the filter specification the user will indicate which
 * the threshold for the filter.  Value above the threshold will
 * be passed, values below the threshold will be dropped.
 *
 * \param ohandle
 * 		An object handle for the object to process.
 *
 * \param filter_args
 * 		The data structure that was returned from the filter
 *		initialization function.
 *
 */
typedef int (*filter_eval_proto)(lf_obj_handle_t ohandle, void *filter_args);



/*!
 * This is the prototype for a filter init function.
 *
 * This function should take the filter arguments allocated in the
 * filter_init_proto() and free them.  
 *
 * The fini function should return 0 if it cleaned up correctly.  Any
 * other value is treated as a fatal condition.
 *
 * \param filter_args
 * 		The data structure that was returned from the filter
 *		initialization function.
 */

typedef int (*filter_fini_proto)(void *filter_args);


/*!
 * This gets the next blocks of data in an object.  The block size is
 * specified by the filter specification.  
 *
 *
 * \param obj_handle
 *		the object handle.
 *
 * \param num_blocks
 *		the number of blocks to read.
 *
 * \param lenp
 *		This is a pointer to the number of bytes to read.  On
 *		return this location will hold the number of bytes that
 *		were actually read.
 *
 * \param bufp
 *		A pointer to the location where the buffer pointer
 * 		will be stored.
 *
 * \return 0
 *		the read was successful. 
 *
 * \return ENOSPC
 *		Insufficient resources were available to complete the call.
 * 
 * \return ENOENT
 *		No more data is available.
 *
 * \return EINVAL
 *		One of the parameters was invalid. 
 *
 */

int lf_next_block(lf_obj_handle_t obj_handle, int num_blocks,  
			size_t *lenp, unsigned char **bufp);

/*!
 * This skips the next N blocks of the specified object.  For an input
 * object, this will effect the data retrieved the next time lf_next_block()
 * is called.  XXX specify what happens on output objects if we aren't
 * currently aligned on the block boundary.
 *
 *
 * \param obj_handle
 *		The object handle 
 *
 * \param num_blocks
 *		the number of blocks to skip.
 *
 * \return 0
 *		the skip was successful. 
 *
 * \return ENOSPC
 *		insufficient resources were available to complete the call.
 * 
 * \return EINVAL
 *		one of the handles was invalid. 
 */

int lf_skip_block(lf_obj_handle_t obj_handle, int num_blocks);

/*!
 * Read an attribute from the object into the buffer space provided
 * by the caller.  This does invoke a copy and for large structures
 * it is preferable to use lf_ref_attr() if the caller will not
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

int lf_read_attr(lf_obj_handle_t ohandle, const char *name, size_t *len, 
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

int lf_ref_attr(lf_obj_handle_t ohandle, const char *name, 
		size_t *len, unsigned char **data);


/*!
 * This function sets the some of the object's attributes.
 *
 * \param ohandle
 *		the object handle.
 *
 * \param name
 *		The name of the attribute to write.
 *
 * \param len
 *		The length of the attribute to write.
 *
 * \param data
 *		A pointer of the data associated with the data.
 *
 * \return 0
 *		Attributes were written successfully.
 *
 * \return EPERM
 * 		Filter does not have permission to write these
 *		attributes.
 *
 * \return EINVAL
 *		One or more of the arguments was invalid.
 */

int lf_write_attr(lf_obj_handle_t ohandle, char *name, size_t len, 
		unsigned char *data);


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

void lf_log(int level, char *fmt, ...);


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

int lf_first_attr(lf_obj_handle_t ohandle, char **name, 
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

int lf_next_attr(lf_obj_handle_t ohandle, char **name, 
		size_t *len, unsigned char **data, void **cookie);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_FILTER_H_  */
