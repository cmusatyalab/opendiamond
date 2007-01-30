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
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <assert.h>
#include <dirent.h>
#include <stdint.h>
#include "ring.h"
#include "lib_searchlet.h"
#include "lib_dctl.h"
#include "lib_od.h"
#include "lib_log.h"
#include "lib_odisk.h"
#include "filter_exec.h"
#include "dctl.h"
#include "lib_tools.h"
#include "idiskd_ops.h"


static char const cvsid[] = "$Header$";


/* linux specific flag */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define	DCTL_RING_SIZE	512

/* create a buffer much larger than we need */
#define	BIG_SIZE	512
static	char 		data_buffer[BIG_SIZE];

#define	MAX_ENTS	128
static	dctl_entry_t	entry_buffer[MAX_ENTS];


static void
send_err_response(int conn, int err_code)
{
	dctl_msg_hdr_t	msg;
	int		err;


	msg.dctl_op = DCTL_OP_REPLY;
	msg.dctl_err = err_code;
	msg.dctl_dlen = 0;
	msg.dctl_plen = 0;


	err = send(conn, &msg, sizeof(msg), MSG_NOSIGNAL);
	/* XXX ignore response */

}


static void
send_read_response(int conn, dctl_data_type_t dtype, int len, char *data_buffer)
{
	dctl_msg_hdr_t	msg;
	int		err;

	msg.dctl_op = DCTL_OP_REPLY;
	msg.dctl_dtype = dtype;
	msg.dctl_err = 0;
	msg.dctl_dlen = len;
	msg.dctl_plen = 0;

	err = send(conn, &msg, sizeof(msg), MSG_NOSIGNAL);
	if (err != sizeof(msg)) {
		return;
	}
	/* XXX ignore response */

	err = send(conn, data_buffer, len, MSG_NOSIGNAL);
	if (err != len) {
		return;
	}
}

static void
send_list_response(int conn, int num_ents, dctl_entry_t *entry_buffer)
{
	dctl_msg_hdr_t	msg;
	int		err;
	int		dlen;

	dlen = num_ents * sizeof(dctl_entry_t);

	msg.dctl_op = DCTL_OP_REPLY;
	msg.dctl_err = 0;
	msg.dctl_dlen = dlen;
	msg.dctl_plen = 0;

	err = send(conn, &msg, sizeof(msg), MSG_NOSIGNAL);
	if (err!=sizeof(msg)) {
		return;
	}
	/* XXX ignore response */

	err = send(conn, (char *)entry_buffer, dlen, MSG_NOSIGNAL);
	if (err != dlen) {
		return;
	}
}



static void
process_request(dctl_msg_hdr_t *msg, char *data, int conn)
{
	int		        err;
	int		        len;
	dctl_op_t	    cmd;
	char *		    path;
	char *		    arg;
	int		        arg_len;
	dctl_data_type_t    dtype;

	path = data;

	cmd = msg->dctl_op;

	printf("dctl: proc_request: %d \n", cmd);
	switch(cmd) {
		case DCTL_OP_READ:
			len = BIG_SIZE;
			err = dctl_read_leaf(path, &dtype, &len, data_buffer);
			assert(err != ENOMEM);
			if (err) {
				send_err_response(conn, err);
			}
			send_read_response(conn, dtype, len, data_buffer);
			break;

		case DCTL_OP_WRITE:
			arg = &data[msg->dctl_plen];
			arg_len = msg->dctl_dlen - msg->dctl_plen;
			err = dctl_write_leaf(path, arg_len, arg);
			assert(err != ENOMEM);
			send_err_response(conn, err);
			break;

		case DCTL_OP_LIST_NODES:
			len = MAX_ENTS;
			err = dctl_list_nodes(path, &len, entry_buffer);
			assert(err != ENOMEM);
			if (err) {
				send_err_response(conn, err);
			}
			send_list_response(conn, len, entry_buffer);
			break;

		case DCTL_OP_LIST_LEAFS:
			len = MAX_ENTS;
			err = dctl_list_leafs(path, &len, entry_buffer);
			assert(err != ENOMEM);
			if (err) {
				send_err_response(conn, err);
			}
			send_list_response(conn, len, entry_buffer);
			break;

		default:
			assert(0);
			break;

	}

}


void
process_dctl_requests(search_state_t *sc, int conn)
{
	dctl_msg_hdr_t	msg;
	char *		buf;
	int		len, dlen;

	while(1) {
		/*
		 * Look to see if there is any control information to
		 * process.
		 */
		len = recv(conn, &msg, sizeof(msg), MSG_WAITALL);
		if (len != sizeof(msg)) {
			return;
		}

		dlen = msg.dctl_dlen;
		buf = (char *)malloc(dlen);
		assert(buf != NULL);

		len = recv(conn, buf, dlen, MSG_WAITALL);
		if (len != dlen) {
			return;
		}
		process_request(&msg, buf, conn);
	}
}




/*
 * The main loop that the background thread runs to process
 * the data coming from the individual devices.
 */

static void *
dctl_main(void *arg)
{
	search_state_t *	sc;
	int			err;
	int	fd, newsock;
	char 	user_name[MAX_USER_NAME];
	struct sockaddr_un sa;
	struct sockaddr_un newaddr;
	int	slen;

	/* change the umask so someone else can delete
	 * the socket later.
	 */
	umask(0);

	sc = (search_state_t *)arg;

	dctl_thread_register(sc->dctl_cookie);
	log_thread_register(sc->log_cookie);


	/*
	 * Open the socket for the log information.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	/* XXX socket error code */

	/* bind the socket to a path name */
	get_user_name(user_name);
	sprintf(sa.sun_path, "%s.%s", SOCKET_DCTL_NAME, user_name);
	sa.sun_family = AF_UNIX;
	unlink(sa.sun_path);

	err = bind(fd, (struct sockaddr *)&sa, sizeof (sa));
	if (err < 0) {
		fprintf(stderr, "binding %s\n", sa.sun_path);
		perror("bind failed ");
		exit(1);
	}

	if (listen(fd, 5) == -1) {
		perror("listen failed ");
		exit(1);

	}


	while (1) {
		slen = sizeof(newaddr);
		if ((newsock = accept(fd, (struct sockaddr *)&newaddr, &slen))
		    == -1) {

			perror("accept failed \n");
			continue;
		}

		process_dctl_requests(sc, newsock);
		close(newsock);
	}
}


int
dctl_start(search_state_t *sc)
{

	int		err;
	pthread_t	thread_id;
#ifdef	XXX
	/*
	 * Initialize the ring of commands for the thread.
	 */
	err = ring_init(&sc->dctl_ring, DCTL_RING_SIZE);
	if (err) {
		/* XXX err log */
		return(err);
	}
#endif

	/*
	 * Create a thread to handle background processing.
	 */
	err = pthread_create(&thread_id, PATTR_DEFAULT, dctl_main, (void *)sc);
	if (err) {
		/* XXX log */
		printf("failed to create background thread \n");
		return(ENOENT);
	}
	return(0);
}

