/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _LIB_FILTER_PRIV_H_
#define	_LIB_FILTER_PRIV_H_

typedef struct
{
	int	min_ver;
	int	maj_ver;
	int	num_free;
	int	num_malloc;
}
filter_lib_handle_t;



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


diamond_public
int lf_ref_attr_no_callback(lf_obj_handle_t ohandle, const char *name, 
			    size_t *len, unsigned char **data);

#endif /* _LIB_FILTER_PRIV_H_  */
