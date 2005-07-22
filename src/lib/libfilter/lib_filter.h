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

#ifndef _LIB_FILTER_H_
#define	_LIB_FILTER_H_

/*
 * These are the set of functions available to to filters running
 * on the system.
 */


#include <sys/types.h>		/* for off_t */

#ifdef __cplusplus
extern "C"
{
#endif


/*
 * The handle to a an object.
 */
typedef	void *	lf_obj_handle_t;


/*
 * Some constants to define the latest/greatest API version.
 */
#define	LF_LATEST_MAJOR_VERSION		1
#define	LF_LATEST_MINOR_VERSION		0


/*
 * This is the prototype for a filter init function.
 *
 * This function takes a list of filter arguments as well as
 * a blob of data.  The init function then allocates any data structures
 * and sets filter_args to point to the allocated data.
 *
 * The init function should return 0 if it initialized correction.  Any
 * other value is treated as a fatal condition.
 */

typedef int (*filter_init_proto)(int num_arg, char **args, int bloblen,
				 void *blob_data, void **filter_args);


/*
 * This is the prototype for a filter function.
 *
 * This function is a varargs function that takes an object
 * handle a set of configured arguments (through the filter
 * specification).
 *
 * The return value is an integer that represents a confidence.  As
 * part of the filter specification the user will indicate which
 * the threshold for the filter.  Value above the threshold will
 * be passed, values below the threshold will be dropped.
 */

typedef int (*filter_eval_proto)(lf_obj_handle_t in_handle, void *filter_args);


/*
 * This is the prototype for a filter init function.
 *
 * This function should the filter arguments allocated in the
 * filter_init_proto() and free them.  
 *
 * The fini function should return 0 if it initialized correction.  Any
 * other value is treated as a fatal condition.
 */

typedef int (*filter_fini_proto)(void *filter_args);


/*
 * This gets the next blocks of data in an object.  The block size is
 * specified by the filter specification.  
 *
 *
 * Args:
 * 	obj_handle - the object handle.
 *
 * 	num_blocks - the number of blocks to read.
 *
 * 	lenp	   - This is a pointer to the number of bytes to read.  On
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
 * 	ENOENT	   - insufficient resources were available to complete the call.
 * 
 * 	EINVAL     - one of the handles was invalid. 
 *
 */

int lf_next_block(lf_obj_handle_t obj_handle, int num_blocks,  
			off_t *lenp, char **bufp);

/*
 * This skips the next N blocks of the specified object.  For an input
 * object, this will effect the data retrieved the next time lf_next_block()
 * is called.  XXX specify what happens on output objects if we aren't
 * currently aligned on the block boundary.
 *
 *
 * Args:
 * 	obj_handle - the object handle.
 *
 * 	num_blocks - the number of blocks to read.
 *
 * Return:
 * 	0	   - the read was successful. 
 *
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 * 
 * 	EINVAL     - one of the handles was invalid. 
 *
 */

int lf_skip_block(lf_obj_handle_t obj_handle, int num_blocks);

/*
 * This writes a range of data to an object.  After this call, the
 * library takes possession of the buffer, the caller should not attempt
 * any further accesses to the buffer or free the buffer.
 *
 * Args:
 * 	obj_handle - the object handle.
 *
 * 	flags 	   - Specifies specific behavior of the write.
 * 		     If the flag LF_WRITE_BLOCK_PAD is set, then
 * 		     the write is padded out to the next block
 * 		     size specified for the object in the filter spec.
 *
 * 	len	   - the number of bytes to write.
 *
 * 	buf	   - The buffer to write.
 *
 * Return:
 * 	0	   - the write was successful. 
 *
 * 	EINVAL     - one of the handles was invalid. 
 *
 * 	EPERM      - The filter does not have ability to write to the data
 *		     (may be object, etc).
 */

#define	LF_WRITE_BLOCK_PAD	0x01
int lf_write_block(lf_obj_handle_t obj_handle, int flags, off_t len, char *buf);


/*
 * This allocates memory for the application to use.  This should be
 * used for allocating buffers to hold object data as well as other 
 * user data that is typically called through malloc.
 *
* Args:
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
 *
 */
int lf_alloc_buffer(off_t len, char **buf);


/*
 * This frees a buffer that was allocated through a read call or
 * or lf_alloc_buffer().
 *
 * Args:
 * 	buf	   - The buffer to release.
 *
 * Return:
 * 	0	   - the write was successful. 
 * 
 * 	ENOSPC	   - insufficient resources were available to complete the call.
 *
 * 	EINVAL     - one of the handles was invalid. 
 */
int lf_free_buffer(char *buf);

/*
 * This function reads the some of the object's attributes.
 *
 * Args:
 * 	obj_handle - the object handle.
 *
 * 	name       - The name of the attribute to read.
 *
 * 	len 	   - A pointer to the location where the length
 * 		     of the data storage is stored.  The caller
 * 		     sets this to the amount of space allocated.
 * 		     Upon return this is set to the size of
 * 		     data actually read.  If there is not enough
 * 		     space then ENOSPC is returned and the value is
 * 		     updated to the size needed to successfully complete
 * 		     the call.
 *
 * 	data 	   - The location where the results should be stored.
 *
 * Return:
 *	0	   - Attributes were read successfully.
 *
 * 	ENOSPC	   - data was not large enough to complete the write.  
 * 		     length should be set to the size need to complete
 * 		     the read.     
 *
 *	EPERM	   - Filter does not have permission to write these
 *		     attributes.
 *
 *	EINVAL     - One or more of the arguments was invalid.
 */

int lf_read_attr(lf_obj_handle_t obj, const char *name, off_t *len, char *data);

/* XXX lh add comments */
int lf_ref_attr(lf_obj_handle_t obj, const char *name, off_t *len, char **data);

/*
 * This function sets the some of the object's attributes.
 *
 * Args:  
 * 	obj_handle - the object handle.
 *
 * 	name       - The name of the attribute to write.
 *
 * 	len 	   - The length of the attribute to write.
 *
 * 	data 	   - A pointer of the data associated with the data.
 *
 * Return:
 *	0	   - Attributes were written successfully.
 *
 *	EPERM	   - Filter does not have permission to write these
 *		     attributes.
 *
 *	EINVAL     - One or more of the arguments was invalid.
 */

int lf_write_attr(lf_obj_handle_t obj, char *name, off_t len, char *data);


/*
 * This function allows the programmer to log some data that
 * can be retrieved from the host system.
 *
 * Args:  
 * 	level 	   - The log level associated with the command.  This
 * 		     used to limit the amount of information being passed.
 *
 * 	name       - The name of the attribute to write.
 *
 * 	fmt	   - format string used for parsing the data.  This uses
 * 		     printf syntax
 *
 * 	... 	   - the arguments for the format.
 *
 */

int lf_log(int level, char *fmt, ...);


/*
 * Debug function used by the runtime, we probably should move it
 * elsewhere.
 */
typedef void (*read_attr_cb)(lf_obj_handle_t ohandle,
			     const char *name, off_t len, const char *data);
int lf_set_read_cb(read_attr_cb);

typedef void (*write_attr_cb)(lf_obj_handle_t ohandle, const char *name, 
		off_t len, const char *data);

int lf_set_write_cb(write_attr_cb);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_FILTER_H_  */
