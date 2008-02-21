/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include "lib_auth.h"

/* stubs to intentionally fail any code that assumes it can reliably authenticate */
auth_handle_t auth_conn_server(int sockfd)
{
	return NULL;
}

auth_handle_t auth_conn_client_ext(int sockfd, char *service)
{
	return NULL;
}

auth_handle_t auth_conn_client(int sockfd)
{
	return auth_conn_client_ext(sockfd, DIAMOND_SERVICE);
}

int auth_msg_encrypt(auth_handle_t handle, char *inbuf, int ilen,
					 char *outbuf, int olen)
{
	return -1;
}

int auth_msg_decrypt(auth_handle_t handle, char *inbuf, int ilen,
					char *outbuf, int olen)
{
	return -1;
}

