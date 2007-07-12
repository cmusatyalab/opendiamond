/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <assert.h>
#include <stdlib.h>
#include <tirpc/rpc/types.h>
#include <rpc/rpc.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_auth.h"
#include "lib_hstub.h"
#include "hstub_impl.h"
#include "ports.h"
#include "rpc_client_content.h"


/*
 * set a socket to non-blocking 
 */
static void
socket_non_block(int fd)
{
	int             flags,
	                err;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: issue fcntl");
		return;
	}
	err = fcntl(fd, F_SETFL, (flags | O_NONBLOCK));
	if (err == -1) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: failed to set fcntl");
		return;
	}
}


static int
create_tcp_connection(uint32_t devid, uint16_t port)
{
	int sockfd;
	struct sockaddr_in sa;
	
	if(!devid || !port) return -1;

	/* create TCP socket */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror("socket");
	  return -1;
	}

	/* define destination */	
	sa.sin_family = AF_INET;
	sa.sin_port = port;
	sa.sin_addr.s_addr = (in_addr_t) devid; /* already in network order */
	
	/* make connection */
	if(connect(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
	  perror("connect");
	  return -1;
	}
	
	return sockfd;
}

/* tirpc_init() takes an already-connected socket file descriptor and
   makes it into a TI-RPC client handle */

static CLIENT *
tirpc_init(int connfd) {
	struct sockaddr control_name;
	unsigned int control_name_len = sizeof(struct sockaddr);
	struct netconfig *nconf;
	struct netbuf *tbind;
	CLIENT *clnt;

	nconf = getnetconfigent("tcp");
	if(nconf == NULL) {
	  perror("getnetconfigent");
	  return NULL;
	}
	
	/* Transform sockaddr_in to netbuf */
	tbind = (struct netbuf *) malloc(sizeof(struct netbuf));
	if(tbind == NULL) {
	  perror("malloc");
	  return NULL;
	}
	
	tbind->buf = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	if(tbind->buf == NULL) {
	  perror("malloc");
	  return NULL;
	}
	if(getsockname(connfd, &control_name, &control_name_len) < 0) {
	  perror("getsockname");
	  return NULL;
	}
	memcpy(tbind->buf, &control_name, sizeof(struct sockaddr));
	tbind->maxlen = tbind->len = sizeof(struct sockaddr);
	
	if ((clnt = clnt_tli_create(connfd,
				    nconf,
				    tbind, 
				    CLIENTCONTENT_PROG, 
				    CLIENTCONTENT_VERS, 
				    BUFSIZ, BUFSIZ)) == NULL) {
	  clnt_pcreateerror("clnt_tli_create");
	  return NULL;
	}

	free(tbind->buf);
	free(tbind);
	freenetconfigent(nconf);

	return clnt;
}


static int 
data_connect(uint32_t devid, uint16_t portnum, unsigned int session_nonce) {
	int connfd, size;

	connfd = create_tcp_connection(devid, portnum);
	if (connfd < 0){
	  log_message(LOGT_NET, LOGL_ERR, "data_connect: create_tcp_connection() failed");
	  return(-1);
	}

	/* Write the 32-bit session_nonceback to the server. */
	size = writen(connfd, (char *) &session_nonce, sizeof(session_nonce));
	if (size < 0) {
	  log_message(LOGT_NET, LOGL_ERR, "data_connect: Failed writing session_nonce");
	  close(connfd);
	  return(-1);
	}

	return connfd;
}


static int
control_connect(uint32_t devid, uint16_t portnum, unsigned int *session_nonce) {
	int connfd, size;
	
	connfd = create_tcp_connection(devid, portnum);
	if (connfd < 0){
	  log_message(LOGT_NET, LOGL_ERR, "control_connect: create_tcp_connection() failed");
	  return(-1);
	}
	
	/* Read 32-bit cookie from server. */
	size = readn(connfd, session_nonce, sizeof(*session_nonce));
	if (size < 0) {
	  log_message(LOGT_NET, LOGL_ERR, "control_connect: Failed reading session_nonce");
	  close(connfd);
	  return(-1);
	}
	
	return connfd;
}

/*
 * Create and establish a socket with the other
 * side.
 */
int
hstub_establish_connection(conn_info_t *cinfo, uint32_t devid)
{
	ssize_t         size, len;
	int		auth_required = 0;
	char		buf[BUFSIZ];


        uint16_t px = htons(diamond_get_control_port());
	cinfo->dev_id = devid;
	cinfo->control_fd = control_connect(devid, px, &cinfo->session_nonce);
	if (cinfo->control_fd < 0 ) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: failed to initialize control connection");
		return(ENOENT);
	}


	if ((int) cinfo->session_nonce < 0) {
		/* authenticate */
		cinfo->ca_handle = auth_conn_client(cinfo->control_fd);
		if (cinfo->ca_handle) {
			/* wait for ack to our connect request */
			size = readn(cinfo->control_fd, &buf[0], BUFSIZ);
			if (size == -1) {
			  log_message(LOGT_NET, LOGL_ERR, "hstub_establish_connection: failed to read encrypted session_nonce");
			  close(cinfo->control_fd);
			  return (ENOENT);
			}
	

			/* decrypt the message */	
			len = auth_msg_decrypt(cinfo->ca_handle, buf, size, 
									(char *) &cinfo->session_nonce, 
									sizeof(cinfo->session_nonce));
			if (len < 0) {
			  log_message(LOGT_NET, LOGL_ERR, "hstub_establish_connection: failed to decrypt session_nonce");
				close(cinfo->control_fd);
				return (ENOENT);
			}
			
			auth_required = 1;
			cinfo->flags |= CINFO_AUTHENTICATED;
		} else {			
			log_message(LOGT_NET, LOGL_ERR, 
		    	"hstub_establish_connection: auth_conn_client() failed");
			close(cinfo->control_fd);
			return (ENOENT);
		}
	}


	/*
	 * Now we open the data socket and send the cookie on it.
	 */

        px = htons(diamond_get_data_port());
        cinfo->data_fd = data_connect(devid, px, cinfo->session_nonce);
	if (cinfo->data_fd <0) {
		log_message(LOGT_NET, LOGL_ERR, 
		    "hstub: connect data port failed");
		close(cinfo->control_fd);
		return (ENOENT);
	}

	/* authenticate connection */
	if (auth_required) {
		cinfo->da_handle = auth_conn_client(cinfo->data_fd);
		if (cinfo->da_handle) {
			/* encrypt the cookie */
			len = auth_msg_encrypt(cinfo->da_handle,  
							(char *) &cinfo->session_nonce, 
							sizeof(cinfo->session_nonce),
							buf, BUFSIZ);
			if (len < 0) {
				printf("failed to encrypt message");
				close(cinfo->control_fd);
				close(cinfo->data_fd);
				return (ENOENT);
			}
			
			/* send the cookie */
			size = write(cinfo->data_fd, buf, len);
			if (size == -1) {
				log_message(LOGT_NET, LOGL_ERR, 
		    				"hstub: send on  data port failed");
				close(cinfo->control_fd);
				close(cinfo->data_fd);
				return (ENOENT);
			}
		} else {
			log_message(LOGT_NET, LOGL_ERR, 
		    	"hstub: failed to read from socket");
			close(cinfo->control_fd);
			close(cinfo->data_fd);
			return (ENOENT);
		}
	} 

	cinfo->tirpc_client = tirpc_init(cinfo->control_fd);
	if (cinfo->tirpc_client == NULL) {
		log_message(LOGT_NET, LOGL_ERR, 
		    "hstub: TI-RPC initialization failed");
		close(cinfo->control_fd);
		close(cinfo->data_fd);
		return (ENOENT);
	}

	socket_non_block(cinfo->data_fd);

	/*
	 * Set the state machines variables.
	 */
	cinfo->control_state = CONTROL_TX_NO_PENDING;
	cinfo->control_rx_state = CONTROL_RX_NO_PENDING;
	cinfo->data_rx_state = DATA_RX_NO_PENDING;

	return (0);
}
