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

#ifndef _LIB_FILTER_H_
#define	_LIB_FILTER_H_

/*
 * These are the set of functions available to to filters running
 * on the system.
 */


#include <sys/types.h>		/* for off_t */

#ifdef __cplusplus
extern "C" {
#endif


/*
 * The handle to a an object.
 */
typedef	void *	lf_obj_handle_t;


/*
 * The handle to an instance of the library.
 */
typedef void * 	lf_fhandle_t;


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

typedef int (*filter_init_proto)(int num_arg, char **args, int app_bloblen,
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

typedef int (*filter_eval_proto)(lf_obj_handle_t in_handle, int num_outhandle,
	     lf_obj_handle_t *out_handlev, void *filter_args);


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
 * This is called to initialize an instance of the filter library.  It
 * returns a handle that will be used for subsequent calls to the library.
 *
 * Args:
 * 	major_version -	The major version of the library that the application
 * 			wishes to use. 
 *
 * 	minor_version -	The minor version of the library that the application
 * 			wishes to use.  
 *
 * Return:
 * 	NULL	      - Unable to initialize the library.  This will be caused
 * 			when the supported version is not available.
 *
 *   non-NULL	      - This will be the handle used for subsequent library
 *   			calls.
 *
 */

lf_fhandle_t lf_init_lib(int major_version, int minor_version);


/*
 * This is called to initialize closes a version of the library
 * that was being managed.  
 *
 * Args:
 * 	fhandle       - a open handle to the library.
 *
 * Return:
 * 	0	      - The call was successful.
 *	EINVAL	      - The handle was not valid.
 *
 */

int lf_close_lib(lf_fhandle_t *fhandle);

/*
 * This gets the next blocks of data in an object.  The block size is
 * specified by the filter specification.  
 *
 *
 * Args:
 * 	fhandle    - a handle to the instance of the library.
 *
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

int lf_next_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle, 
			int num_blocks,  off_t *lenp, char **bufp);

/*
 * This skips the next N blocks of the specified object.  For an input
 * object, this will effect the data retrieved the next time lf_next_block()
 * is called.  XXX specify what happens on output objects if we aren't
 * currently aligned on the block boundary.
 *
 *
 * Args:
 * 	fhandle    - a handle to the instance of the library.
 *
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

int lf_skip_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle, 
			int num_blocks);

/*
 * This writes a range of data to an object.  After this call, the
 * library takes possession of the buffer, the caller should not attempt
 * any further accesses to the buffer or free the buffer.
 *
 * Args:
 * 	fhandle    - a handle to the instance of the library.
 *
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
int lf_write_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle,
			int flags, off_t len, char *buf);


/*
 * This allocates memory for the application to use.  This should be
 * used for allocating buffers to hold object data as well as other 
 * user data that is typically called through malloc.
 *
 * Args:
 * 	fhandle    - a handle to the instance of the library.
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
 *
 */
int lf_alloc_buffer(lf_fhandle_t fhandle, off_t len, char **buf);


/*
 * This frees a buffer that was allocated through a read call or
 * or lf_alloc_buffer().
 *
 * Args:
 * 	fhandle    - a handle to the instance of the library.
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
int lf_free_buffer(lf_fhandle_t fhandle, char *buf);

/*
 * This function reads the some of the object's attributes.
 *
 * Args:
 * 	fhandle    - a handle to the instance of the library.
 *
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

int lf_read_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj,  
		    	const char *name, off_t *len, char *data);

/* XXX lh add comments */
int lf_ref_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj,  
		    	const char *name, off_t *len, char **data);

/*
 * This function sets the some of the object's attributes.
 *
 * Args:  
 * 	fhandle    - a handle to the instance of the library.
 *
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

int lf_write_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj,  
			char *name, off_t len, char *data);


/*
 * This function allows the programmer to log some data that
 * can be retrieved from the host system.
 *
 * Args:  
 * 	fhandle    - a handle to the instance of the library.
 *
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

int lf_log(lf_fhandle_t fhandle, int level, char *fmt, ...);


/* 
 * Debug function used by the runtime, we probably should move it
 * elsewhere.
 */
typedef void (*read_attr_cb)(char *fhandle, uint64_t obj_id, const char *name, off_t len, const char *data);
int lf_set_read_cb(read_attr_cb);

typedef void (*write_attr_cb)(char *fhandle, uint64_t obj_id, const char *name, off_t len, const char *data);
int lf_set_write_cb(write_attr_cb);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_FILTER_H_  */
