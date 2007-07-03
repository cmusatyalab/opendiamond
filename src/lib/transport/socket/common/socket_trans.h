/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _SOCKET_TRANS_H_
#define _SOCKET_TRANS_H_

#include "diamond_consts.h"
#include "sig_calc.h"
#include "rpc_preamble.h"


#define	OBJ_MAGIC_HEADER	0x54124567
typedef struct object_header {
	uint32_t	obj_magic;	/* for debugging */
	uint32_t	attr_len;	/* length of the attributes */
	uint32_t	data_len;	/* length of the data portion */
	uint32_t	remain_compute;	/* remaining compute * 1000 */
	uint32_t	version_num;	/* search version number */
} obj_header_t;


/*
 * Header that goes on the log buffers that are sent to the host.
 */
#define	LOG_MAGIC_HEADER	0x54122756

typedef struct log_header {
	uint32_t	log_magic;	/* for debugging */
	uint32_t	log_len;	/* length log data */
} log_header_t;


/*
 * credit count state that is passed in the reverse direction
 * on the credit count channel.
 */
#define	CC_MAGIC_HEADER		0x81204570
typedef struct credit_count_msg {
	uint32_t	cc_magic;	/* for debugging */
	uint32_t	cc_count;	/* number of object can send */
} credit_count_msg_t;


ssize_t readn(int fd, void *vptr, size_t n); 
ssize_t writen(int fd, const void *vptr, size_t n); 
char *  diamond_error(diamond_rc_t *rc);

#endif /* _SOCKET_TRANS_H_ */
