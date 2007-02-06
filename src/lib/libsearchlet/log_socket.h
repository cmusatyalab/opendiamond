/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_LOG_SOCKET_H_
#define	_LOG_SOCKET_H_

#define	SOCKET_LOG_NAME		"/tmp/log_socket"


/*
 * This is the message header that is sent to
 * the host stub.
 */

typedef	enum {
    LOG_SOURCE_BACKGROUND = 1,
    LOG_SOURCE_DEVICE
} source_type_t;



typedef struct {
	int		log_len;
	source_type_t	log_type;
	int		dev_id;
} log_msg_t;


typedef enum {
    LOG_SETLEVEL_ALL = 1,
    LOG_SETLEVEL_DEVICE,
    LOG_SETLEVEL_HOST
} log_level_op_t;


typedef struct {
	log_level_op_t	log_op;		/* the op in host order */
	uint32_t	log_level;	/* the level in network order */
	uint32_t	log_src;	/* the source flags in network order */
	uint32_t	dev_id;		/* device id for LOG_SETLEVEL_HOST */
} log_set_level_t;


#endif /* !_LOG_SOCKET_H_ */
