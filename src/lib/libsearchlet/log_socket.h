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

#ifndef	_LOG_H_
#define	_LOG_H_

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


#endif /* !_LOG_H_ */
