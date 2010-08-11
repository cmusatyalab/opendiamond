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

#ifndef _LIB_FILTER_SYS_H_
#define	_LIB_FILTER_SYS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Debug function used by the runtime, we probably should move it
 * elsewhere.
 */
typedef void (*read_attr_cb)(lf_obj_handle_t ohandle, const char *name, 
		off_t len, const unsigned char *data);

int lf_set_read_cb(read_attr_cb);

typedef void (*write_attr_cb)(lf_obj_handle_t ohandle, const char *name, 
		off_t len, const unsigned char *data);

int lf_set_write_cb(write_attr_cb);


#ifdef __cplusplus
}
#endif

#endif /* _LIB_FILTER_SYS_H_  */
