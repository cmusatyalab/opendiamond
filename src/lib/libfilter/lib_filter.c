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

/*
 * This provides many of the main functions in the provided
 * through the searchlet API.
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include "ring.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_searchlet.h"
#include "lib_search_priv.h"
#include "lib_filter.h"
#include "lib_filter_priv.h"
#include "obj_attr_dump.h"
#include "filter_exec.h"
#include "lib_log.h"

static read_attr_cb 	read_attr_fn = NULL;
static write_attr_cb 	write_attr_fn = NULL;

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

lf_fhandle_t
lf_init_lib(int major_version, int minor_version)
{

	filter_lib_handle_t *	fdata;
	/*
	 * We currently only support the latest version.  If the
	 * callers isn't requesting this version, then we fail.
	 */
	if ((major_version != LF_LATEST_MAJOR_VERSION) &&
	    (minor_version != LF_LATEST_MINOR_VERSION)) {
		return(NULL);
	}


	fdata = (filter_lib_handle_t *)malloc(sizeof(*fdata));
	if (fdata == NULL) {
		/* XXX log */
		return(NULL);
	}

	fdata->min_ver = LF_LATEST_MINOR_VERSION;
	fdata->maj_ver = LF_LATEST_MAJOR_VERSION;
	fdata->num_free = 0;
	fdata->num_malloc = 0;


	return((lf_fhandle_t)fdata);

}


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

int
lf_set_read_cb(read_attr_cb cb_fn)
{
	read_attr_fn = cb_fn;
	return(0);
}

/*
 * Quick hacks for now.  Fix this later.
 * XXX
 */
/* need to pass in fhandle as the filter name */
int
lf_read_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj, const char *name,
             off_t *len, char *data)
{
	obj_data_t	*odata;
	obj_attr_t	*adata;
	int		err;

	odata = (obj_data_t *)obj;
	adata = &odata->attr_info;
	err = obj_read_attr(adata, name, len, data);
	/* add read attrs into cache queue: input attr set */
	if ( !err && (read_attr_fn != NULL) ) {
		(*read_attr_fn)((char *)fhandle, odata->local_id, name, *len, data);
	}
	//if( !err )
	//	ocache_add_iattr((char *)fhandle, odata->local_id, name, *len, data);
	return(err);
}


int
lf_ref_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj, const char *name,
             off_t *len, char **data)
{
	obj_data_t	*odata;
	obj_attr_t	*adata;
	int		err;

	odata = (obj_data_t *)obj;
	adata = &odata->attr_info;
	err = obj_ref_attr(adata, name, len, data);
	/* add read attrs into cache queue: input attr set */
	if (!err && (read_attr_fn != NULL)) {
		(*read_attr_fn)((char *)fhandle, odata->local_id, name, *len, *data);
	}
	//if( !err )
	//	ocache_add_iattr((char *)fhandle, odata->local_id, name, *len, data);
	return(err);
}

/* XXX */
int
lf_dump_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj)
{
	obj_data_t	*odata;
	obj_attr_t	*adata;
	int		err;

	odata = (obj_data_t *)obj;
	adata = &odata->attr_info;
	err = obj_dump_attr(adata);
	return(err);
}


int
lf_set_write_cb(write_attr_cb cb_fn)
{
	write_attr_fn = cb_fn;
	return(0);
}

/*
 * Quick hacks for now.  Fix this later.
 * XXX
 */
int
lf_write_attr(lf_fhandle_t fhandle, lf_obj_handle_t obj, char *name, off_t len,
              char *data)
{
	obj_data_t	*odata;
	obj_attr_t	*adata;
	int		err;

	odata = (obj_data_t *)obj;
	adata = &odata->attr_info;
	err = obj_write_attr(adata, name, len, data);
	/* add writen attrs into cache queue: output attr set */
	if ( !err && (write_attr_fn != NULL) ) {
		(*write_attr_fn)((char *)fhandle, odata->local_id, name, len, data);
	}
	//if( !err )
	//	ocache_add_oattr((char *)fhandle, odata->local_id, name, len, data);
	return(err);
}

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

int
lf_next_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle,
              int num_blocks,  off_t *len, char **bufp)
{
	obj_data_t *	odata;
	char	*	buf;
	off_t		length;
	off_t		remain;
	int		err;

	odata = (obj_data_t *)obj_handle;

	/*
	 * See if there is any data to read.
	 */
	if (odata->data_len <= odata->cur_offset) {
		printf("too much dat %016llX  off %lx len %lx \n",
		       odata->local_id, odata->cur_offset, odata->data_len);
		*len = 0;
		assert(0);
		return(ENOENT);
	}

	/* XXX check for off by 1 errors here */
	length = num_blocks * odata->cur_blocksize;
	remain = odata->data_len - odata->cur_offset;
	if (length > remain) {
		length = remain;
	}

	err = lf_alloc_buffer(fhandle, length, &buf);
	if (err) {
		printf("failed to allocate block \n");
		assert(0);
		*len = 0;
		return(err);
	}

	memcpy(buf, &odata->data[odata->cur_offset], length);

	odata->cur_offset += length;
	*len = length;
	*bufp = buf;

	return (0);
}


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

int
lf_skip_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle, int num_blocks)
{
	obj_data_t *	odata;
	off_t		length;
	off_t		remain;

	odata = (obj_data_t *)obj_handle;

	/*
	 * See if there is any data to read.  XXX for write, see
	 * if we should do something different !!!
	 */
	if (odata->data_len <= odata->cur_offset) {
		return(ENOENT);
	}

	length = num_blocks * odata->cur_blocksize;
	remain = odata->data_len - odata->cur_offset;

	if (length > remain) {
		length = remain;
	}

	odata->cur_offset += length;
	return (0);
}

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
int
lf_write_block(lf_fhandle_t fhandle, lf_obj_handle_t obj_handle,
               int flags, off_t len, char *buf)
{

	return(EINVAL);
}


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
int
lf_alloc_buffer(lf_fhandle_t fhandle, off_t len, char **buf)
{

	char * new_buf;
	/* XXX for now we just do malloc, put some
	 * debugging code in here for later.
	 */
	new_buf = (char *)malloc(len);
	if (new_buf == NULL) {
		return(ENOSPC);
	}

	*buf = new_buf;

	return(0);
}



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
int
lf_free_buffer(lf_fhandle_t fhandle, char *buf)
{

	/* XXX keep some states and error checking */
	free(buf);
	return 0;
}




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
/* XXX this should match the one in log, but doesn't need to */
#define	MAX_LOG_BUF	80

int
lf_log(lf_fhandle_t fhandle, int level, char *fmt, ...)
{
	va_list	ap;
	va_list	new_ap;
	char	log_buffer[MAX_LOG_BUF];
	char	*cur_filter;
	int	len;
	int	remain_len;

	cur_filter = fexec_cur_filtname();
	len = snprintf(log_buffer, MAX_LOG_BUF, "%s : ", cur_filter);
	assert((len > 0) || (len < MAX_LOG_BUF));

	remain_len = MAX_LOG_BUF - len;

	va_start(ap, fmt);
	va_copy(new_ap, ap);
	vsnprintf(&log_buffer[len], remain_len, fmt, new_ap);
	va_end(ap);

	log_buffer[MAX_LOG_BUF - 1] = '\0';

	log_message(LOGT_APP, level, "%s", log_buffer);

	return(0);
}
