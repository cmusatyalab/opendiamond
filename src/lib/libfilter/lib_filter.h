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
#define	LF_LATEST_MAJOR_VERSION		0
#define	LF_LATEST_MINOR_VERSION		5


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

typedef int (*filter_proto)(lf_obj_handle_t in_handle, int num_outhandle,
	     lf_obj_handle_t *out_handlev, int num_arg, char **args);


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

extern lf_fhandle_t lf_init_lib(int major_version, int minor_version);


/*
 * This is called to initialize closes a version of the library
 * that was being mananged.  
 *
 * Args:
 * 	fhandle       - a open handle to the library.
 *
 * Return:
 * 	0	      - The call was successful.
 *	EINVAL	      - The handle was not valid.
 *
 */

extern int lf_close_lib(lf_fhandle_t *fhandle);

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
 * 	ENOENT	   - insufficient resources were available to complete the call.
 * 
 * 	EINVAL     - one of the handles was invalid. 
 *
 */

extern int lf_next_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle, 
			int num_blocks,  off_t *len, char **bufp);

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

extern int lf_skip_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle, 
			int num_blocks);

/*
 * This writes a range of data to an object.  After this call, the
 * library takes possesion of the buffer, the caller should not attempt
 * any further acceses to the buffer or free the buffer.
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
extern int lf_write_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle,
			int flags, off_t len, char *buf);


/*
 * This allocated a range of memory for the application to use.  This should be
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
extern int lf_alloc_buffer(lf_fhandle_t fhandle, off_t len, char **buf);


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
extern int lf_free_buffer(lf_fhandle_t fhandle, char *buf);

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
 * 		     data actually read.  If there is not enought
 * 		     space then ENOSPC is returned and the value is
 * 		     updated to the size needed to sucessfully complete
 * 		     the call.
 *
 * 	data 	   - The location where the results should be stored.
 *
 * Return:
 *	0	   - Attributes were read sucessfully.
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

extern int lf_read_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj,  
		    	char *name, off_t *len, char *data);

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
 *	0	   - Attributes were written sucessfully.
 *
 *	EPERM	   - Filter does not have permission to write these
 *		     attributes.
 *
 *	EINVAL     - One or more of the arguments was invalid.
 */

extern int lf_write_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj,  
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

extern int lf_log(lf_fhandle_t fhandle, int level, char *fmt, ...);


/*
 * a hack to dump the attribute names to stdout. Tries to print out
 * the values as well, if it can locate a print method */
/* XXX */
extern int lf_dump_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj);

#ifdef __cplusplus
}
#endif

#endif /* _LIB_FILTER_H_  */
