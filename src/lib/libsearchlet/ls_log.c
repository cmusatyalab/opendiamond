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
#include "lib_log.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "filter_exec.h"
#include "log.h"
#include "log_impl.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_tools.h"

#define	LOG_RING_SIZE	512

/* linux specific flag */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/*
 * send a specific log entry.
 */

int
log_send_local_data(search_context_t *sc, int conn)
{

	log_msg_t	log_msg;
	char *		data_buf;
	int		data_len;
	int		slen;
	int		err = 0;


	/*
	 * Get any data, if there isn't any, then return.
	 */
	data_len = log_getbuf(&data_buf);
	if (data_len == 0) {
		return (0);
	}

	/*
	 * Fill in the header.
	 */

	log_msg.log_len = data_len;
	log_msg.log_type = LOG_SOURCE_BACKGROUND;
	/* XXX should we get our IP?? */
	log_msg.dev_id = 0;


	slen = send(conn, (void *)&log_msg, sizeof(log_msg), MSG_NOSIGNAL);
	if ((slen < 0) || (slen != sizeof(log_msg))) {
		err = 1;
		goto done;
	}


	slen = send(conn, (void *)data_buf, data_len, MSG_NOSIGNAL);
	if ((slen < 0) || (slen != data_len)) {
		err = 1;
		goto done;
	}

done:
	log_advbuf(data_len);
	return(err);
}

/*
 * send a specific log entry.
 */

int
log_send_queued_data(search_context_t *sc, log_info_t *log_info, int conn)
{

	log_msg_t	log_msg;
	char *		data;
	int		data_len;
	int		slen;
	int		err = 0;

	data = log_info->data;
	data_len = log_info->len;


	log_msg.log_len = data_len;
	/* XXX set type */
	log_msg.log_type = LOG_SOURCE_DEVICE;
	log_msg.dev_id = log_info->dev;


	slen = send(conn, (void *)&log_msg, sizeof(log_msg), MSG_NOSIGNAL);
	if ((slen < 0) || (slen != sizeof(log_msg))) {
		err = 1;
		goto done;
	}


	slen = send(conn, (void *)data, data_len, MSG_NOSIGNAL);
	if ((slen < 0) || (slen != data_len)) {
		err = 1;
		goto done;
	}

done:
	free(data);
	free(log_info);
	return(err);

}


static uint32_t		last_level, last_src;

void
update_device_log_level(search_context_t *sc)
{
	device_handle_t	*	cur_dev;
	int					err;
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {

		err = device_set_log(cur_dev->dev_handle, last_level, last_src);
	}

}


void
set_device_log(log_set_level_t *llevel, search_context_t *sc)
{
	device_handle_t *cur_dev;
	uint32_t	hlevel, hsrc;
	int		err;

	hlevel = ntohl(llevel->log_level);
	hsrc = ntohl(llevel->log_src);

	switch(llevel->log_op) {
		case LOG_SETLEVEL_ALL:
			last_level = llevel->log_level;
			last_src = llevel->log_src;

			for (cur_dev = sc->dev_list; cur_dev != NULL;
			     cur_dev = cur_dev->next) {

				err = device_set_log(cur_dev->dev_handle,
				                     llevel->log_level, llevel->log_src);
			}

			log_setlevel(hlevel);
			log_settype(hsrc);

			break;

		case LOG_SETLEVEL_DEVICE:
			cur_dev = sc->dev_list;
			while(cur_dev != NULL) {
				if (cur_dev->dev_id == llevel->dev_id) {
					err = device_set_log(
					          cur_dev->dev_handle,
					          llevel->log_level,
					          llevel->log_src);
				}
				cur_dev = cur_dev->next;
			}
			break;


		case LOG_SETLEVEL_HOST:
			log_setlevel(hlevel);
			log_settype(hsrc);
			break;


		default:
			assert(0);
			break;

	}


	/* XXX handle single device set options */


}


void
process_log_data(search_context_t *sc, int conn)
{
	log_info_t *linfo;
	log_set_level_t	llevel;
	int	next_sec = 0;
	struct timeval	curtime;
	struct timezone	tz;
	int err;
	int	len;

	while(1) {

		/*
		 * This code looks at the current time, and if 1 second
		 * has passed since the last time, it will try to update
		 * the log levels on each of the storage devices.  This
		 * deals with the case that we did not have a connection
		 * with the storage device when the log level was initially
		 * set.
		 */
		err = gettimeofday(&curtime, &tz);
		assert(err == 0);

		if (curtime.tv_sec >= next_sec) {
			update_device_log_level(sc);
			next_sec = curtime.tv_sec + 1;
		}


		/*
		 * Look to see if there is any control information to
		 * process.
		 */
		len = recv(conn, &llevel, sizeof(llevel), MSG_DONTWAIT);
		if (len == sizeof (llevel) ) {
			set_device_log(&llevel, sc);
		} else if (len > 0) {
			/* if we got something other than an error
			 * we have a partial we don't handle, so assert.
			 * XXX should we do something smarter ?? 
			 */
			assert(0);
		} else if ((len == -1) && (errno != EAGAIN)) {
			return;
		} else if (len == 0) {
			return;
		}



		/* XXX get local log data also */
		linfo = (log_info_t *) ring_deq(sc->log_ring);
		if (linfo == NULL) {
			sleep(1);
		} else {
			err = log_send_queued_data(sc, linfo, conn);
			if (err) {
				return;
			}
		}
		err = log_send_local_data(sc, conn);
		if (err) {
			return;
		}
	}
}



/*
 * The main loop that the background thread runs to process
 * the data coming from the individual devices.
 */

static void *
log_main(void *arg)
{
	search_context_t *	sc;
	int			err;
	int	fd, newsock;
	char	user_name[MAX_USER_NAME];
	struct sockaddr_un sa;
	struct sockaddr_un newaddr;
	int	slen;

	/* change the umask so someone else can delete
	 * the socket later.
	 */
	umask(0);

	sc = (search_context_t *)arg;

	dctl_thread_register(sc->dctl_cookie);
	log_thread_register(sc->log_cookie);

	/*
	 * Open the socket for the log information.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	/* XXX socket error code */

	/* bind the socket to a path name */
	get_user_name(user_name);
	sprintf(sa.sun_path, "%s.%s", SOCKET_LOG_NAME, user_name);
	sa.sun_family = AF_UNIX;
	unlink(sa.sun_path);

	err = bind(fd, (struct sockaddr *)&sa, sizeof (sa));
	if (err < 0) {
		fprintf(stderr, "binding %s\n", SOCKET_LOG_NAME);
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

		process_log_data(sc, newsock);
		close(newsock);
	}
}


int
log_start(search_context_t *sc)
{

	int		err;
	pthread_t	thread_id;

	/*
	 * Initialize the ring of commands for the thread.
	 */
	err = ring_init(&sc->log_ring, LOG_RING_SIZE);
	if (err) {
		/* XXX err log */
		return(err);
	}

	/*
	 * Create a thread to handle background processing.
	 */
	err = pthread_create(&thread_id, PATTR_DEFAULT, log_main, (void *)sc);
	if (err) {
		/* XXX log */
		printf("failed to create background thread \n");
		return(ENOENT);
	}
	return(0);
}

