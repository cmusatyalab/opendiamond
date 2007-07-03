/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib_auth.h"   
 

void usage() {
	printf("usage: actest <host_name> <port_number>\n");
	exit(-1);
}

int main(int argc, char **argv)
{
	struct protoent *pent;
	struct sockaddr_in sa;
	struct hostent *hent;
	int             err;
	size_t          size;
	size_t			len;
	int				sockfd;
	uint32_t		cookie;
	auth_handle_t	handle;
	char			buf[BUFSIZ];
	
	if (argc < 3) {
		usage();
	}

	/* open a socket */
	pent = getprotobyname("tcp");
	if (pent == NULL) {
		printf("failed to find tcp");
		return(ENOENT);
	}

	if ((sockfd = socket(PF_INET, SOCK_STREAM,pent->p_proto))<0){
		printf("failed to create socket");
		return(ENOENT);
	}

	/* get the remote host address */
	hent = gethostbyname(argv[1]);
	if (hent == NULL) {
		printf("failed to get hostname\n");
		return(ENOENT);
	}
	
	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short) atoi(argv[2]));
	bcopy(hent->h_addr, &(sa.sin_addr.s_addr), hent->h_length);

	err = connect(sockfd, (struct sockaddr *) &sa, sizeof(sa));
	if (err) {
		printf("connect failed: %d", err);
		return (err);
	}
	
	/* authenticate connection */
	handle = auth_conn_client(sockfd);
	if (handle == NULL) {
		printf("authentication failed");
		return (ENOENT);
	}

	/* wait for ack to our connect request */
	size = read(sockfd, &buf[0], BUFSIZ);
	if (size == -1) {
		printf("failed to read from socket");
		return (ENOENT);
	}
	
	printf("Read encrypted message of %d bytes\n", size);

	/* decrypt the message */	
	len = auth_msg_decrypt(handle, buf, size, 
							(char *) &cookie, sizeof(cookie));
	if (len < 0) {
		printf("failed to decrypt message");
		return (ENOENT);
	}
	
	printf("Received cookie: %d\n", cookie);
	
	return(0);
}

