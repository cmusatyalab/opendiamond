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
#include <sys/un.h>
#include <assert.h>
#include "ring.h"
#include "lib_searchlet.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "filter_exec.h"
#include "log.h"
#include "log_impl.h"
#include "assert.h"



/*
 * send a specific log entry.
 */

int
log_send_data(search_context_t *sc, log_info_t *log_info, int conn)
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

void
process_log_data(search_context_t *sc, int conn)
{
	log_info_t *linfo;
	int err;

	while(1) {
		/* XXX get local log data also */
		linfo = (log_info_t *) ring_deq(sc->log_ring);
		if (linfo == NULL) {
			sleep(1);
		} else {
			err = log_send_data(sc, linfo, conn);
			if (err)
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
	struct sockaddr_un sa;
	struct sockaddr_un newaddr;
	int	slen;


	sc = (search_context_t *)arg;

	/*
	 * Open the socket for the log information.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	/* XXX socket error code */

	/* bind the socket to a path name */

	strcpy(sa.sun_path, SOCKET_LOG_NAME);
	sa.sun_family = AF_UNIX;
	unlink(sa.sun_path);

	err = bind(fd, (struct sockaddr *)&sa, sizeof (sa));
	if (err < 0) {
		perror("connect failed ");
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
	err = ring_init(&sc->log_ring);
	if (err) {
		/* XXX err log */
		return(err);
	}

	/*
	 * Create a thread to handle background processing.
	 */
	err = pthread_create(&thread_id, NULL, log_main, (void *)sc);
	if (err) {
		/* XXX log */
		printf("failed to create background thread \n");
		return(ENOENT);
	}
	return(0);
}

