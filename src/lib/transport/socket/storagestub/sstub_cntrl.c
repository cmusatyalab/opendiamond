/*
 * 	Diamond (Release 1.0)
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
#include <sys/socket.h>
#include <sys/stat.h>
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
#include "lib_sstub.h"
#include "sstub_impl.h"


static char const cvsid[] = "$Header$";



void
sstub_write_control(listener_state_t *lstate, cstate_t *cstate)
{
	control_header_t *	cheader;
	int			header_remain, header_offset;
	int			data_remain, data_offset;
	char *			data;
	int			send_len;


	/* clear the flags for now, there is no more data */

	if (cstate->control_tx_state == CONTROL_TX_NO_PENDING) {
		/*
		 * If we get here we are are not currently sending
		 * any message.  See if there is another item on the
		 * ring.  If not, then we clear the CSTATE_CONTROL_DATA
		 * to indicate we should not be called until more data
		 * is queued.  Otherwise we setup the header and data
		 * offsets and remain counters as appropriate.
		 */
		pthread_mutex_lock(&cstate->cmutex);
		cheader = (control_header_t *)ring_deq(cstate->control_tx_ring);
		if (cheader == NULL) {
			cstate->flags &= ~CSTATE_CONTROL_DATA;
			pthread_mutex_unlock(&cstate->cmutex);
			return;
		}
		pthread_mutex_unlock(&cstate->cmutex);

		header_remain = sizeof(*cheader);
		header_offset = 0;
		data_remain = ntohl(cheader->data_len);
		data_offset = 0;

	} else if (cstate->control_tx_state == CONTROL_TX_HEADER) {
		/*
		 * If we got here we were in the middle of sending
		 * the control message header when we ran out of
		 * buffer space in the socket.  Setup the offset and
		 * remaining counts for the header and the data portion.\
		 */

		cheader = cstate->control_tx_header;
		header_offset = cstate->control_tx_offset;
		header_remain = sizeof(*cheader) - header_offset;
		;
		data_remain = ntohl(cheader->data_len);
		data_offset = 0;


	} else  {
		/*
		 * If we get here, then we are in the middle of transmitting
		 * the data associated with the command.  Setup the data
		 * offsets and remain counters.
		 */
		assert(cstate->control_tx_state == CONTROL_TX_DATA);

		cheader = cstate->control_tx_header;
		header_offset = 0;
		header_remain = 0;
		data_offset = cstate->control_tx_offset;
		data_remain =  ntohl(cheader->data_len) - data_offset;
	}


	/*
	 * If we have any header data remaining, then go ahead and send it.
	 */
	if (header_remain != 0) {
		data = (char *)cheader;
		send_len = send(cstate->control_fd, &data[header_offset],
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
				cstate->control_tx_header = cheader;
				cstate->control_tx_offset = header_offset;
				cstate->control_tx_state = CONTROL_TX_HEADER;
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
			cstate->control_tx_header = cheader;
			cstate->control_tx_offset = header_offset + send_len;
			cstate->control_tx_state = CONTROL_TX_HEADER;
			return;
		}
	}




	if (data_remain != 0) {
		data = (char *)cheader->spare;
		send_len = send(cstate->control_fd, &data[data_offset],
		                data_remain, 0);

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
				cstate->control_tx_header = cheader;
				cstate->control_tx_offset = data_offset;
				cstate->control_tx_state = CONTROL_TX_DATA;
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
		if (send_len != data_remain) {
			cstate->control_tx_header = cheader;
			cstate->control_tx_offset = data_offset + send_len;
			cstate->control_tx_state = CONTROL_TX_DATA;
			return;
		}

		/* if we get here, then the data was sent */
		free(data);
	}

	/* some stats */
	cstate->stats_control_tx++;
	cstate->stats_control_bytes_tx += sizeof(*cheader) +
	                                  ntohl(cheader->data_len);



	/*
	 * We sent the whole message so reset our state machine and
	 * release the message header.
	 */
	cstate->control_tx_state = CONTROL_TX_NO_PENDING;
	free(cheader);
	return;
}



void
sstub_except_control(listener_state_t *lstate, cstate_t *cstate)
{
	printf("XXX except_control \n");
	/* Handle the case where we are shutting down */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}

	return;
}

void
process_searchlet_message(listener_state_t *lstate, cstate_t *cstate,
                          char *data)
{
	char *			lib_name;
	char *			spec_name;
	uint32_t		cmd;
	uint32_t		gen;
	searchlet_subhead_t *	shead;
	int			spec_offset, filter_offset;
	int			lib_fd, spec_fd;
	int			spec_len, filter_len;
	size_t			wsize;

	cmd = ntohl(cstate->control_rx_header.command);
	gen = ntohl(cstate->control_rx_header.generation_number);

	shead = (searchlet_subhead_t *)data;
	spec_offset = sizeof(*shead);
	spec_len = ntohl(shead->spec_len);
	filter_len = ntohl(shead->filter_len);
	filter_offset = ((spec_len + 3) & ~3) + spec_offset;

	/*
	 * create a file for storing the searchlet library.
	 */

	/* sanity check the constants */
	assert(MAX_TEMP_NAME > (sizeof(TEMP_DIR_NAME) +
	                        sizeof(TEMP_OBJ_NAME) + 1));

	lib_name = (char *)malloc (MAX_TEMP_NAME);
	if (lib_name == NULL) {
		printf("filename failed \n");
		exit(1);
	}
	sprintf(lib_name, "%s%s", TEMP_DIR_NAME, TEMP_OBJ_NAME);
	umask(0000);

	lib_fd = mkstemp(lib_name);
	if (lib_fd == -1) {
		free(lib_name);
		/* XXX */
		perror("failed to create lib_fd:");
		/* XXX */
		exit(1);
	}

	if (spec_len > 0) {
		/*
	 	 * Create a file to hold the filter spec.
	 	 */
		assert(MAX_TEMP_NAME > (sizeof(TEMP_DIR_NAME) +
	                        sizeof(TEMP_SPEC_NAME) + 1));

		spec_name = (char *)malloc (MAX_TEMP_NAME);
		if (spec_name == NULL) {
			printf("specname failed \n");
			exit(1);
		}
		sprintf(spec_name, "%s%s", TEMP_DIR_NAME, TEMP_SPEC_NAME);


		spec_fd = mkstemp(spec_name);
		if (spec_fd == -1) {
			free(spec_name);
			/* XXX */
			printf("failed to create spec_fd \n");
			/* XXX */
			exit(1);
		}

		wsize = write(spec_fd, &data[spec_offset], (size_t) spec_len);
		if (wsize != spec_len) {
			printf("write %d len %d err %d \n", wsize, spec_len,
		       		errno);
			assert(0);
		}
		assert(wsize == spec_len);
		close(spec_fd);
	} else {
		spec_name = NULL;
	}

	wsize = write(lib_fd, &data[filter_offset], (size_t) filter_len);
	assert(filter_len == filter_len);
	close(lib_fd);


	(*lstate->set_searchlet_cb)(cstate->app_cookie, gen,
	                            lib_name, spec_name);

}

/*
 * This is called when we have an indication that data is ready
 * on the control port.
 */

static void
process_control(listener_state_t *lstate, cstate_t *cstate, char *data)
{
	uint32_t		cmd;
	uint32_t		gen;

	cmd = ntohl(cstate->control_rx_header.command);
	gen = ntohl(cstate->control_rx_header.generation_number);

	switch (cmd) {
		case CNTL_CMD_START:
			(*lstate->start_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_STOP:
			(*lstate->stop_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_SET_SEARCHLET:
			assert(data != NULL);
			process_searchlet_message(lstate, cstate, data);
			free(data);
			break;

		case CNTL_CMD_SET_LIST:
			/* XXX this needs to be expanded */
			(*lstate->set_list_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_TERMINATE:
			(*lstate->terminate_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_GET_STATS:
			(*lstate->get_stats_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_GET_CHAR:
			(*lstate->get_char_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_SETLOG: {
				setlog_subheader_t	*sheader;
				assert(data != NULL);
				sheader = (setlog_subheader_t *)data;
				(*lstate->setlog_cb)(cstate->app_cookie,
				                     sheader->log_level, sheader->log_src);
				free(data);
				break;
			}

		case CNTL_CMD_READ_LEAF: {
				dctl_subheader_t *   shead;
				int32_t              opid;
				assert(data != NULL);

				shead = (dctl_subheader_t *)data;
				opid = ntohl(shead->dctl_opid);

				(*lstate->rleaf_cb)(cstate->app_cookie, shead->dctl_data, opid);
				free(data);
				break;
			}


		case CNTL_CMD_WRITE_LEAF: {
				dctl_subheader_t *   shead;
				int32_t              opid;
				int                  dlen, plen;
				assert(data != NULL);

				shead = (dctl_subheader_t *)data;
				opid = ntohl(shead->dctl_opid);
				dlen = ntohl(shead->dctl_dlen);
				plen = ntohl(shead->dctl_plen);

				(*lstate->wleaf_cb)(cstate->app_cookie, shead->dctl_data,
				                    dlen, &shead->dctl_data[plen], opid);

				free(data);
				break;
			}

		case CNTL_CMD_LIST_NODES: {
				dctl_subheader_t *   shead;
				int32_t              opid;
				assert(data != NULL);

				shead = (dctl_subheader_t *)data;
				opid = ntohl(shead->dctl_opid);

				(*lstate->lnode_cb)(cstate->app_cookie, shead->dctl_data, opid);
				free(data);
				break;
			}


		case CNTL_CMD_LIST_LEAFS: {
				dctl_subheader_t *   shead;
				int32_t              opid;
				assert(data != NULL);

				shead = (dctl_subheader_t *)data;
				opid = ntohl(shead->dctl_opid);

				(*lstate->lleaf_cb)(cstate->app_cookie, shead->dctl_data, opid);
				free(data);
				break;
			}

		case CNTL_CMD_ADD_GID: {
				sgid_subheader_t *   shead;
				groupid_t           gid;

				assert(data != NULL);
				shead = (sgid_subheader_t *)data;

				gid = shead->sgid_gid; /* XXX 64bit bswap */
				(*lstate->sgid_cb)(cstate->app_cookie, gen, gid);
				free(data);
				break;
			}

		case CNTL_CMD_CLEAR_GIDS:
			assert(data == NULL);
			(*lstate->clear_gids_cb)(cstate->app_cookie, gen);
			break;

		case CNTL_CMD_SET_BLOB: {
				blob_subheader_t *  shead;
				void *				blob;
				int					blen;
				int					nlen;
				char *				name;

				assert(data != NULL);
				shead = (blob_subheader_t *)data;

				nlen = ntohl(shead->blob_nlen);
				blen = ntohl(shead->blob_blen);
				name = shead->blob_sdata;
				blob = &shead->blob_sdata[nlen];

				(*lstate->set_blob_cb)(cstate->app_cookie, gen, name, blen, blob);
				free(data);
				break;
			}

		case CNTL_CMD_SET_OFFLOAD: {
				offload_subheader_t *   shead;
				uint64_t				val;

				assert(data != NULL);
				shead = (offload_subheader_t *)data;

				val = shead->offl_data; /* XXX 64bit bswap */
				(*lstate->set_offload_cb)(cstate->app_cookie, gen, val);
				free(data);
				break;
			}

		default:
			printf("unknown command: %d \n", cmd);
			if (data) {
				free(data);
			}
			break;
	}


	return;
}


/*
 * This reads data from the control socket.  If we get enough data to 
 * complete a full message, then we will get a full control
 * message, then we will pass the message to be processed.  Note
 * that this code relies on state stored in the cstate and we
 * must process data in order that it recieves, so this function should
 * only be called by a single thread.
 */

void
sstub_read_control(listener_state_t *lstate, cstate_t *cstate)
{
	int			dsize;
	char *	    data;
	char *		data_buf = NULL;
	int			remain_header, remain_data;
	int			header_offset, data_offset;

	/* Handle the case where we are shutting down */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		printf("read control:  shutting down \n");
		return;
	}


	/*
	 * Handle the different cases where we may already
	 * be processing data.
	 */
	if (cstate->control_rx_state == CONTROL_RX_NO_PENDING) {
		remain_header = sizeof(cstate->control_rx_header);
		header_offset = 0;
		remain_data = 0;
		data_offset = 0;

	} else if (cstate->control_rx_state == CONTROL_RX_HEADER) {

		header_offset = cstate->control_rx_offset;
		remain_header = sizeof(cstate->control_rx_header) -
		                header_offset;
		remain_data = 0;
		data_offset = 0;
	} else {
		assert(cstate->control_rx_state == CONTROL_RX_DATA);
		remain_header = 0;
		header_offset = 0;
		data_offset = cstate->control_rx_offset;
		remain_data = ntohl(cstate->control_rx_header.data_len) -
		              data_offset;
		data_buf = cstate->control_rx_data;
	}



	if (remain_header > 0) {
		data = (char *)&cstate->control_rx_header;
		dsize = recv(cstate->control_fd, (char *)&data[header_offset],
		             remain_header, 0);

		/*
			 * Handle some of the different error cases.
			 */
		if (dsize < 1) {
			/*
				 * If we got here the select said there was data, but
				 * the return is 0,  this indicates the connection has
				 * been closed.  We need to shutdown the connection.
				 */
			if (dsize == 0) {
				shutdown_connection(lstate, cstate);
				return;
			}

			/*
			 * The call failed, the only possibility is that
			 * we didn't have enough data for it.  In that
			 * case we return and retry later.
			 */
			if (errno == EAGAIN) {
				/*
				 * nothing has happened, so we should not
				 * need to update the state.
				 */
				return;
			} else {
				/*
				 * some un-handled error happened, we
				 * just shutdown the connection.
				 */
				shutdown_connection(lstate, cstate);
				return;
			}
		}


		/*
		 * Look at how much data we recieved, if we didn't get
		 * enough to complete the header, then we store
		 * the partial state and return, the next try will
		 * get rest of the header.
		 */
		if (dsize != remain_header) {
			cstate->control_rx_offset = header_offset + dsize;
			cstate->control_rx_state = CONTROL_RX_HEADER;
			return;
		}

		/*
		 * If we fall through here, then we have the full header,
		 * so we need to setup the parameters for reading the data
		 * portion.
		 */
		data_offset = 0;
		remain_data = ntohl(cstate->control_rx_header.data_len);

		/* get a buffer to store the data if appropriate */
		if (remain_data > 0) {
			data_buf = (char *) malloc(remain_data);
			if (data_buf == NULL) {
				/* XXX crap, how do I get out of this */
				printf("failed malloc on buffer \n");
				exit(1);
			}
		}

	}


	/*
	 * Now we try to get the remaining data associated with this
	 * command.
	 */
	if (remain_data > 0) {
		dsize = recv(cstate->control_fd, &data_buf[data_offset],
		             remain_data, 0);

		if (dsize < 1) {
			/*
				 * If we got here the select said there was data, but
				 * the return is 0,  this indicates the connection has
				 * been closed.  We need to shutdown the connection.
				 */
			if (dsize == 0) {
				shutdown_connection(lstate, cstate);
				return;
			}

			/*
			 * The call failed, the only possibility is that
			 * we didn't have enough data for it.  In that
			 * case we return and retry later.
			 */
			if (errno == EAGAIN) {
				cstate->control_rx_offset = data_offset;
				cstate->control_rx_data = data_buf;
				cstate->control_rx_state = CONTROL_RX_DATA;
				return;
			} else {
				/*
				 * some un-handled error happened, we
				 * just shutdown the connection.
				 */
				shutdown_connection(lstate, cstate);
				return;
			}
		}


		/*
		 * Look at how much data we recieved, if we didn't get
		 * enough to complete the header, then we store
		 * the partial state and return, the next try will
		 * get rest of the header.
		 */
		if (dsize != remain_data) {
			cstate->control_rx_offset = data_offset + dsize;
			cstate->control_rx_data = data_buf;
			cstate->control_rx_state = CONTROL_RX_DATA;
			return;
		}

	}

	/* update stats */
	cstate->stats_control_rx++;
	cstate->stats_control_bytes_rx += sizeof(cstate->control_rx_header) +
	                                  ntohl(cstate->control_rx_header.data_len);

	/*
	 * Call the process control function.  The caller is responsible
	    * for freeing the allocated data structures.
	    */


	process_control(lstate, cstate, data_buf);

	/* make sure the state is reset */
	cstate->control_rx_state = CONTROL_RX_NO_PENDING;
	return;
}

