/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
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
	ssize_t         size, len;
	int				auth_required = 0;
	char			buf[BUFSIZ];


	unsigned int connectionid;
	int datafd;

        uint16_t px = htons(diamond_get_control_port());
	cinfo->dev_id = devid;
	cinfo->control_fd = control_connect(devid, px, &cinfo->sessionid);
	if (cinfo->control_fd < 0 ) {
		log_message(LOGT_NET, LOGL_ERR, "hstub: failed to initialize control connection");
		return(ENOENT);
	}


	if ((int) cinfo->sessionid < 0) {
		/* authenticate */
		cinfo->ca_handle = auth_conn_client(cinfo->control_fd);
		if (cinfo->ca_handle) {
			/* wait for ack to our connect request */
			size = readn(cinfo->control_fd, &buf[0], BUFSIZ);
			if (size == -1) {
			  log_message(LOGT_NET, LOGL_ERR, "hstub_establish_connection: failed to read encrypted sessionid");
			  close(cinfo->control_fd);
			  return (ENOENT);
			}
	

			/* decrypt the message */	
			len = auth_msg_decrypt(cinfo->ca_handle, buf, size, 
									(char *) &cinfo->sessionid, 
									sizeof(cinfo->sessionid));
			if (len < 0) {
			  log_message(LOGT_NET, LOGL_ERR, "hstub_establish_connection: failed to decrypt sessionid");
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
	cinfo->data_fd = socket(PF_INET, SOCK_STREAM, pent->p_proto);

	/*
	 * we reuse the sockaddr, just change the port number 
	 */
	sa.sin_port = htons(diamond_get_data_port());

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
	 * Set the state machines variables.
	 */
	cinfo->control_state = CONTROL_TX_NO_PENDING;
	cinfo->control_rx_state = CONTROL_RX_NO_PENDING;
	cinfo->data_rx_state = DATA_RX_NO_PENDING;

	return (0);
}




int
create_tcp_connection(uint32_t devid, int port)
{
	int sockfd, error;
	struct sockaddr sa;
	char port_str[6];
	
	if((!devid) || ((port < 0) || (port > 65535)))
	  return -1;

	/* create TCP socket */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror("socket");
	  return -1;
	}
	
	sa.sin_family = AF_INET;
	sa.port = port;
	sa.sin_addr = devid; 
	
	/* make connection */
	if(connect(sockfd, &sa, sizeof(struct sockaddr_in)) < 0) {
	  perror("connect");
	  return -1;
	}
	
	return sockfd;
}


CLIENT *
tirpc_init(int connfd, unsigned int *cookie) {
	struct sockaddr control_name;
	unsigned int control_name_len = sizeof(struct sockaddr);
	struct netconfig *nconf;
	struct netbuf *tbind;
	CLIENT *clnt;

	nconf = getnetconfigent("tcp");
	if(nconf == NULL) {
	  perror("getnetconfigent");
	  exit(EXIT_FAILURE);
	}
	
	/* Transform sockaddr_in to netbuf */
	tbind = (struct netbuf *) malloc(sizeof(struct netbuf));
	if(tbind == NULL) {
	  perror("malloc");
	  exit(EXIT_FAILURE);
	}
	
	tbind->buf = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	if(tbind->buf == NULL) {
	  perror("malloc");
	  exit(EXIT_FAILURE);
	}
	if(getsockname(connfd, &control_name, &control_name_len) < 0) {
	  perror("getsockname");
	  exit(EXIT_FAILURE);
	}
	memcpy(tbind->buf, &control_name, sizeof(struct sockaddr));
	tbind->maxlen = tbind->len = sizeof(struct sockaddr);
	
	if ((clnt = clnt_tli_create(connfd,
					      nconf,
					      tbind, 
					      OPENDIAMOND_PROG, 
					      OPENDIAMOND_VERS, 
					      BUFSIZ, BUFSIZ)) == NULL) {
	  clnt_pcreateerror("clnt_tli_create");
	  fprintf(stderr, "client: error creating TI-RPC tcp client\n");
	  exit(EXIT_FAILURE);
	}
	
	printf("TI-RPC client successfully created and connected!\n");

	return clnt;
}


int 
data_connect(uint32_t devid, uint16_t portnum, unsigned int sessionid) {
	int connfd, size;

	connfd = create_tcp_connection(devid, portnum);
	if (connfd < 0){
	  log_message(LOGT_NET, LOGL_ERR, "data_connect: create_tcp_connection() failed");
	  return(-1);
	}

	/* Write the 32-bit sessionidback to the server. */
	size = writen(connfd, (char *) &sessionid, sizeof(sessionid));
	if (size < 0) {
	  log_message(LOGT_NET, LOGL_ERR, "data_connect: Failed writing sessionid");
	  close(connfd);
	  return(-1);
	}

	return connfd;
}


int
control_connect(uint32_t devid, uint16_t portnum, unsigned int *sessionid) {
	int connfd, size;
	
	connfd = create_tcp_connection(devid, portnum);
	if (connfd < 0){
	  log_message(LOGT_NET, LOGL_ERR, "control_connect: create_tcp_connection() failed");
	  return(-1);
	}
	
	/* Read 32-bit cookie from server. */
	size = readn(connfd, sessionid, sizeof(*sessionid));
	if (size < 0) {
	  log_message(LOGT_NET, LOGL_ERR, "control_connect: Failed reading sessionid");
	  close(connfd);
	  return(-1);
	}
	
	return connfd;
}

ssize_t                         /* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n)
{
  size_t  nleft;
  ssize_t nread;
  char   *ptr;

  ptr = vptr;
  nleft = n;

  while (nleft > 0) {
    if ( (nread = read(fd, ptr, nleft)) < 0) {
      perror("read");
      if (errno == EINTR)
        nread = 0;      /* and call read() again */
      else
        return (-1);
    } else if (nread == 0)
      break;              /* EOF */

    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);         /* return >= 0 */
}

ssize_t                         /* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;   /* and call write() again */
      else
        return (-1);    /* error */
    }

    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n);
}
