/*
 * 	Diamond
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
#ifndef	_DCTL_COMMON_H_
#define	_DCTL_COMMON_H_

#define	SOCKET_DCTL_NAME	"/tmp/dctl_socket"


/*
 * This is the message header that is sent to
 * the host stub.
 */

typedef	enum {
    DCTL_OP_READ = 1,
    DCTL_OP_WRITE,
    DCTL_OP_LIST_NODES,
    DCTL_OP_LIST_LEAFS,
    DCTL_OP_REPLY,
} dctl_op_t;


typedef struct {
	dctl_op_t	        dctl_op;
	dctl_data_type_t    	dctl_dtype;
	uint32_t	        dctl_err;
	uint32_t	        dctl_dlen;
	uint32_t	        dctl_plen;
} dctl_msg_hdr_t;

#endif /* !_DCTL_COMMON_H_ */
