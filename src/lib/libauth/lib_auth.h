/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2006, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef LIB_AUTH_H_
#define LIB_AUTH_H_

#include <netinet/in.h>

#define DIAMOND_SERVICE "diamond"
#define DIAMOND_VERSION "Diamond_protocol_v1.0"

/*
 * handle for authentication context
 */
typedef	void *	auth_handle_t;

auth_handle_t auth_conn_client(int sockfd);
auth_handle_t auth_conn_server(int sockfd);
int auth_msg_encrypt(auth_handle_t handle, char *inbuf, int ilen, 
					char *outbuf, int olen);
int auth_msg_decrypt(auth_handle_t handle, char *inbuf, int ilen, 
					char *outbuf, int olen);

#endif /*LIB_AUTH_H_*/
