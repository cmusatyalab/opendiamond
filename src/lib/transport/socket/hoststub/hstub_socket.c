/*
 *      Diamond
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


static char const cvsid[] =
    "$Header$";

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


/*
 * Create and establish a socket with the other
 * side.
 */
int
hstub_establish_connection(conn_info_t *cinfo, uint32_t devid)
{
	struct protoent *pent;
	struct sockaddr_in sa;
	int             err;
	size_t          size, len;
	int				auth_required = 0;
	char			buf[BUFSIZ];

	pent = getprotobyname("tcp");
	if (pent == NULL) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: failed to find tcp");
		return(ENOENT);
	}

	if ((cinfo->control_fd = socket(PF_INET, SOCK_STREAM,pent->p_proto))<0){
		log_message(LOGT_NET, LOGL_ERR, 
		    "hstub: failed to create socket");
		return(ENOENT);
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons((unsigned short) CONTROL_PORT);
	sa.sin_addr.s_addr = devid;

	/*
	 * save the device id for later use 
	 */
	cinfo->dev_id = devid;

	err = connect(cinfo->control_fd, (struct sockaddr *) &sa, sizeof(sa));
	if (err) {
		log_message(LOGT_NET, LOGL_ERR, 
		    "hstub: connect failed");
		return (ENOENT);
	}
	
	/* wait for ack to our connect request */
	size = read(cinfo->control_fd, (char *) &cinfo->con_cookie,
		    sizeof(cinfo->con_cookie));
	if (size == -1) {
		log_message(LOGT_NET, LOGL_ERR, 
		    	"hstub: failed to read from socket");
		close(cinfo->control_fd);
		return (ENOENT);
	}
	if ((int) cinfo->con_cookie < 0) {
		/* authenticate */
		cinfo->ca_handle = auth_conn_client(cinfo->control_fd);
		if (cinfo->ca_handle) {
			/* wait for ack to our connect request */
			size = read(cinfo->control_fd, &buf[0], BUFSIZ);
			if (size == -1) {
				printf("failed to read from socket");
				close(cinfo->control_fd);
				return (ENOENT);
			}
	
			printf("Read encrypted message of %d bytes\n", size);

			/* decrypt the message */	
			len = auth_msg_decrypt(cinfo->ca_handle, buf, size, 
									(char *) &cinfo->con_cookie, 
									sizeof(cinfo->con_cookie));
			if (len < 0) {
				printf("failed to decrypt message");
				close(cinfo->control_fd);
				return (ENOENT);
			}
			
			auth_required = 1;
			cinfo->flags |= CINFO_AUTHENTICATED;
		} else {			
			log_message(LOGT_NET, LOGL_ERR, 
		    	"hstub: failed to read from socket");
			close(cinfo->control_fd);
			return (ENOENT);
		}
	}

	socket_non_block(cinfo->control_fd);

	/*
	 * Now we open the data socket and send the cookie on it.
	 */
	cinfo->data_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);

	/*
	 * we reuse the sockaddr, just change the port number 
	 */
	sa.sin_port = htons((unsigned short) DATA_PORT);

	err = connect(cinfo->data_fd, (struct sockaddr *) &sa, sizeof(sa));
	if (err) {
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
							(char *) &cinfo->con_cookie, 
							sizeof(cinfo->con_cookie),
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
	} else {
		/* write the cookie into the fd */
		size = write(cinfo->data_fd, (char *) &cinfo->con_cookie,
		    sizeof(cinfo->con_cookie));
		if (size == -1) {
			log_message(LOGT_NET, LOGL_ERR, 
		    	"hstub: send on  data port failed");
			close(cinfo->data_fd);
			close(cinfo->control_fd);
			return (ENOENT);
		}
	}

	socket_non_block(cinfo->data_fd);

	/*
	 * Now we open the log socket and send the cookie on it.
	 */
	cinfo->log_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);

	/*
	 * we reuse the sockaddr, just change the port number 
	 */
	sa.sin_port = htons((unsigned short) LOG_PORT);

	err = connect(cinfo->log_fd, (struct sockaddr *) &sa, sizeof(sa));
	if (err) {
		log_message(LOGT_NET, LOGL_ERR, 
		    "hstub: send on log port failed");
		close(cinfo->control_fd);
		close(cinfo->data_fd);
		return (ENOENT);
	}

	/* authenticate connection */
	if (auth_required) {
		cinfo->la_handle = auth_conn_client(cinfo->log_fd);
		if (cinfo->la_handle) {
			/* encrypt the cookie */
			len = auth_msg_encrypt(cinfo->la_handle,  
							(char *) &cinfo->con_cookie, 
							sizeof(cinfo->con_cookie),
							buf, BUFSIZ);
			if (len < 0) {
				printf("failed to encrypt message");
				close(cinfo->log_fd);
				close(cinfo->control_fd);
				close(cinfo->data_fd);
				return (ENOENT);
			}

			/* send the cookie */
			size = write(cinfo->log_fd, buf, len);
			if (size == -1) {
				log_message(LOGT_NET, LOGL_ERR, 
		    				"hstub: send on  data port failed");
				close(cinfo->log_fd);
				close(cinfo->control_fd);
				close(cinfo->data_fd);
				return (ENOENT);
			}
		} else {
			log_message(LOGT_NET, LOGL_ERR, 
		    			"hstub: failed to read from socket");
			close(cinfo->log_fd);
			close(cinfo->control_fd);
			close(cinfo->data_fd);
			return (ENOENT);
		}
	} else {
		/* write the cookie into the fd */
		size = write(cinfo->log_fd, (char *) &cinfo->con_cookie,
				     sizeof(cinfo->con_cookie));
		if (size == -1) {
			log_message(LOGT_NET, LOGL_ERR, 
					    "hstub: write on log port failed");
			close(cinfo->control_fd);
			close(cinfo->data_fd);
			close(cinfo->log_fd);
			return (ENOENT);
		}
	}

	socket_non_block(cinfo->log_fd);

	/*
	 * Set the state machines variables.
	 */
	cinfo->control_state = CONTROL_TX_NO_PENDING;
	cinfo->control_rx_state = CONTROL_RX_NO_PENDING;
	cinfo->data_rx_state = DATA_RX_NO_PENDING;
	cinfo->log_rx_state = LOG_RX_NO_PENDING;

	return (0);
}
