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

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
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
#include <string.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_dctl.h"
#include "lib_auth.h"
#include "lib_sstub.h"
#include "sstub_impl.h"


void
sstub_write_log(listener_state_t * lstate, cstate_t * cstate)
{
	int             header_remain = 0,
	    header_offset = 0;
	int             log_remain = 0,
	    log_offset = 0;
	char           *data;
	int             send_len;


	/*
	 * clear the flags for now, there is no more data 
	 */

	if (cstate->log_tx_state == LOG_TX_NO_PENDING) {
		/*
		 * look to see if there is a buffer to transmit
		 * if not, then return.
		 */

		pthread_mutex_lock(&cstate->cmutex);
		if (cstate->log_tx_buf == NULL) {
			cstate->flags &= ~CSTATE_LOG_DATA;
			pthread_mutex_unlock(&cstate->cmutex);
			return;
		}
		pthread_mutex_unlock(&cstate->cmutex);

		/*
		 * setup the header and the offsets 
		 */
		cstate->log_tx_header.log_magic = htonl(LOG_MAGIC_HEADER);
		cstate->log_tx_header.log_len = htonl(cstate->log_tx_len);


		header_offset = 0;
		header_remain = sizeof(cstate->log_tx_header);
		log_remain = cstate->log_tx_len;
		log_offset = 0;


	} else if (cstate->log_tx_state == LOG_TX_HEADER) {
		/*
		 * If we got here we were in the middle of sending
		 * the message header when we ran out of
		 * buffer space in the socket.  Setup the offset and
		 * remaining counts for the header and the data portion.\
		 */

		header_offset = cstate->log_tx_offset;
		header_remain = sizeof(cstate->log_tx_header) - header_offset;
		log_remain = cstate->log_tx_len;
		log_offset = 0;


	} else {
		/*
		 * If we get here, then we are in the middle of transmitting
		 * the data associated with the command.  Setup the data
		 * offsets and remain counters.
		 */
		assert(cstate->log_tx_state == LOG_TX_DATA);

		header_offset = 0;
		header_remain = 0;
		log_offset = cstate->log_tx_offset;
		log_remain = cstate->log_tx_len - log_offset;
	}


	/*
	 * If we have any header data remaining, then go ahead and send it.
	 */
	if (header_remain != 0) {
		data = (char *) &cstate->log_tx_header;
		send_len = send(cstate->log_fd, &data[header_offset],
				header_remain, 0);

		if (send_len < 1) {

			/*
			 * If we didn't send any data then the socket has
			 * been closed by the other side, so we just close
			 * this connection.
			 */
			if (send_len == 0) {
				shutdown_connection(lstate, cstate);
				return;
			}


			/*
			 * if we get EAGAIN, the connection is fine
			 * but there is no space.  Keep track of our current
			 * state so we can resume later.
			 */
			if (errno == EAGAIN) {
				cstate->log_tx_offset = header_offset;
				cstate->log_tx_state = LOG_TX_HEADER;
				return;
			} else {
				/*
				 * Other errors indicate the socket is 
				 * no good because 
				 * is no good.  
				 */
				shutdown_connection(lstate, cstate);
				return;
			}
		}



		/*
		 * If we didn't send the full amount of data, then
		 * the socket buffer was full.  We save our state
		 * to keep track of where we need to continue when
		 * more space is available.
		 */

		if (send_len != header_remain) {
			cstate->log_tx_offset = header_offset + send_len;
			cstate->log_tx_state = LOG_TX_HEADER;
			return;
		}
	}




	if (log_remain != 0) {
		data = (char *) cstate->log_tx_buf;
		send_len = send(cstate->log_fd, &data[log_offset],
				log_remain, 0);

		if (send_len < 1) {
			/*
			 * If we didn't send any data then the socket has
			 * been closed by the other side, so we just close
			 * this connection.
			 */
			if (send_len == 0) {
				shutdown_connection(lstate, cstate);
				return;
			}

			/*
			 * if we get EAGAIN, the connection is fine
			 * but there is no space.  Keep track of our current
			 * state so we can resume later.
			 */
			if (errno == EAGAIN) {
				cstate->log_tx_offset = log_offset;
				cstate->log_tx_state = LOG_TX_DATA;
				return;
			} else {
				/*
				 * Other erros indicate the sockets is
				 * no good, (connection has been reset??
				 */
				shutdown_connection(lstate, cstate);
				return;
			}
		}

		/*
		 * If we didn't send the full amount of data, then
		 * the socket buffer was full.  We save our state
		 * to keep track of where we need to continue when
		 * more space is available.
		 */
		if (send_len != log_remain) {
			cstate->log_tx_offset = log_offset + send_len;
			cstate->log_tx_state = LOG_TX_DATA;
			return;
		}

	}


	cstate->stats_log_tx++;
	cstate->stats_log_bytes_tx += cstate->log_tx_len +
	    sizeof(cstate->log_tx_header);

	/*
	 * All the data has been sent, so we update of state machine and
	 * call the callback.
	 */

	/*
	 * note that we need to call clear of the values in cstate because
	 * once we call the callback, the caller can add another entry.  We
	 * avoid the race condition by clearing initializing the state before 
	 * we call the cb. 
	 */

	cstate->log_tx_state = LOG_TX_NO_PENDING;
	data = cstate->log_tx_buf;
	cstate->log_tx_buf = NULL;
	log_remain = cstate->log_tx_len;


	pthread_mutex_lock(&cstate->cmutex);
	cstate->flags &= ~CSTATE_LOG_DATA;
	pthread_mutex_unlock(&cstate->cmutex);



	(*lstate->log_done_cb) (cstate->app_cookie, data, log_remain);
	return;
}



void
sstub_except_log(listener_state_t * lstate, cstate_t * cstate)
{
	printf("XXX except_log \n");
	/*
	 * Handle the case where we are shutting down 
	 */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}

	return;
}



void
sstub_read_log(listener_state_t * lstate, cstate_t * cstate)
{
	/*
	 * Handle the case where we are shutting down 
	 */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}

	return;
}
