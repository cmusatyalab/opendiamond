/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
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
#include "lib_auth.h"
#include "lib_sstub.h"
#include "lib_dconfig.h"
#include "sstub_impl.h"


static char const cvsid[] =
    "$Header$";



void
sstub_write_control(listener_state_t * lstate, cstate_t * cstate)
{
	control_header_t *cheader;
	int             header_remain,
	                header_offset;
	int             data_remain,
	                data_offset;
	char           *data;
	int             send_len;


	/*
	 * clear the flags for now, there is no more data 
	 */

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
		cheader =
		    (control_header_t *) ring_deq(cstate->control_tx_ring);
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


	} else {
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
		data_remain = ntohl(cheader->data_len) - data_offset;
	}


	/*
	 * If we have any header data remaining, then go ahead and send it.
	 */
	if (header_remain != 0) {
		data = (char *) cheader;
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
		data = (char *) cheader->spare;
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

		/*
		 * if we get here, then the data was sent 
		 */
		free(data);
	}

	/*
	 * some stats 
	 */
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
sstub_except_control(listener_state_t * lstate, cstate_t * cstate)
{
	printf("XXX except_control \n");
	/*
	 * Handle the case where we are shutting down 
	 */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		return;
	}

	return;
}

void
process_object_message(listener_state_t * lstate, cstate_t * cstate,
			  char *data)
{
	send_obj_subhead_t *shead;
	int objlen;
	int fd;
	char objname[PATH_MAX];
	sig_val_t data_sig;
	char * sig;
	char *cache;
	char *buf;
	int err;
	uint32_t gen;


	shead = (send_obj_subhead_t *) data;

	gen = ntohl(cstate->control_rx_header.generation_number);


	/* get name to store the object */ 	
	cache = dconf_get_binary_cachedir();
	sig = sig_string(&shead->obj_sig);
	snprintf(objname, PATH_MAX, OBJ_FORMAT, cache, sig);
	free(sig);
	free(cache);

	buf = ((char *)shead) + sizeof(*shead);

	objlen = ntohl(shead->obj_len);

	sig_cal(buf, objlen, &data_sig);
	if (memcmp(&data_sig, &shead->obj_sig, sizeof(data_sig)) != 0) {
		fprintf(stderr, "data doesn't match sig\n");
	}
        /* create the new file */
	file_get_lock(objname);
	fd = open(objname, O_CREAT|O_EXCL|O_WRONLY, 0744);
       	if (fd < 0) {
		file_release_lock(objname);
		if (errno == EEXIST) { 
			return; 
		}
		fprintf(stderr, "file %s failed on %d \n", objname, errno); 
		err = errno;
		return;
	} 
	if (write(fd, buf, objlen) != objlen) {
		perror("write buffer file"); 
	}
	close(fd);
	file_release_lock(objname);

	(*lstate->set_fobj_cb)(cstate->app_cookie, gen, &shead->obj_sig);

}

void
process_obj_message(listener_state_t * lstate, cstate_t * cstate,
			  char *data)
{
	uint32_t gen;
	set_obj_header_t *ohead;
	char objpath[PATH_MAX];
	char * cache;
	char * sig;

	gen = ntohl(cstate->control_rx_header.generation_number);
	ohead = (set_obj_header_t *) data;

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_binary_cachedir();
	sig = sig_string(&ohead->obj_sig);
	snprintf(objpath, PATH_MAX, OBJ_FORMAT, cache, sig);
	free(sig);
	free(cache);

	if (file_exists(objpath)) {
		(*lstate->set_fobj_cb)(cstate->app_cookie, gen, &ohead->obj_sig);

	} else {
		/* XXX ref count before start command */
		cstate->pend_obj++;
		sstub_get_obj(cstate, &ohead->obj_sig);
	}
}


diamond_rc_t *
device_start_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	fprintf(stderr, "have_start pend %d \n", cstate->pend_obj);
	if (tirpc_cstate->pend_obj == 0) {
	  (*tirpc_lstate->start_cb) (tirpc_cstate->app_cookie, gen);
	} else {
	  tirpc_cstate->have_start = 1;
	  tirpc_cstate->start_gen = gen;
	}

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_stop_x_2_svc(u_int gen, stop_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	host_stats_t hstats;

	hstats.hs_objs_received = arg2.host_objs_received;
	hstats.hs_objs_queued = arg2.host_objs_queued;
	hstats.hs_objs_read = arg2.host_objs_read;
	hstats.hs_objs_uqueued = arg2.app_objs_queued;
	hstats.hs_objs_upresented = arg2.app_objs_presented;
	(*tirpc_lstate->stop_cb) (tirpc_cstate->app_cookie, gen, &hstats);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_terminate_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	(*tirpc_lstate->terminate_cb) (tirpc_cstate->app_cookie, gen);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_clear_gids_x_2_svc(u_int gen,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;

	(*tirpc_lstate->clear_gids_cb) (tirpc_cstate->app_cookie, gen);				
	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_new_gid_x_2_svc(u_int gen, groupid_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	groupid_t       gid = arg2;
	
	(*tirpc_lstate->sgid_cb) (tirpc_cstate->app_cookie, gen, gid);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_set_blob_x_2_svc(u_int gen, blob_x arg2, struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	void                *blob;
	int                  blen;
	int                  nlen;
	char                *name;
	
	nlen = arg2.blob_name.blob_name_len;
	blen = arg2.blob_data.blob_data_len;
	name = arg2.blob_name.blob_name_val;
	blob = arg2.blob_data.blob_data_val;
	
	(*tirpc_lstate->set_blob_cb) (tirpc_cstate->app_cookie, gen, 
				      name, blen, blob);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}


diamond_rc_t *
device_set_spec_x_2_svc(u_int gen, spec_file_x arg2,  struct svc_req *rqstp)
{
	static diamond_rc_t  result;
	char specpath[PATH_MAX];
	char * cache;
	char *spec_sig, *spec_data;
	int spec_len;
	int fd;

	spec_len = arg2.data.data_len;

	/*
	 * create a file for storing the searchlet library.
	 */
	umask(0000);

	cache = dconf_get_spec_cachedir();
	spec_sig = sig_string(arg2.sig);
	snprintf(specpath, PATH_MAX, SPEC_FORMAT, cache, spec_sig);
	free(spec_sig);
	free(cache);

	spec_data = arg2.data.data_val;

        /* create the new file */
	file_get_lock(specpath);
	fd = open(specpath, O_CREAT|O_EXCL|O_WRONLY, 0744);
       	if (fd < 0) {
	        int err = errno;
		file_release_lock(specpath);
		if (err == EEXIST) { 
			goto done; 
		}
		fprintf(stderr, "file %s failed on %d \n", specpath, err); 
		result.service_err = DIAMOND_FAILEDSYSCALL;
		result.opcode_err = err;
		return &result;
	}
	if (write(fd, spec_data, spec_len) != spec_len) {
		perror("write buffer file"); 
		result.service_err = DIAMOND_FAILEDSYSCALL;
		result.opcode_err = err;
		return &result;
	}
	close(fd);
	file_release_lock(specpath);

done:
	(*tirpc_lstate->set_fspec_cb)(tirpc_cstate->app_cookie, gen, arg2.sig);

	result.service_err = DIAMOND_SUCCESS;
	return &result;
}

/*
 * This is called when we have an indication that data is ready
 * on the control port.
 */

static void
process_control(listener_state_t * lstate, cstate_t * cstate, char *data)
{
	uint32_t        cmd;
	uint32_t        gen;

	cmd = ntohl(cstate->control_rx_header.command);
	gen = ntohl(cstate->control_rx_header.generation_number);

	switch (cmd) {

	case CNTL_CMD_SET_OBJ:
		assert(data != NULL);
		process_obj_message(lstate, cstate, data);
		free(data);
		break;

	case CNTL_CMD_SET_LIST:
		/*
		 * XXX this needs to be expanded 
		 */
		(*lstate->set_list_cb) (cstate->app_cookie, gen);
		break;

	case CNTL_CMD_GET_STATS:
		(*lstate->get_stats_cb) (cstate->app_cookie, gen);
		break;

	case CNTL_CMD_GET_CHAR:
		(*lstate->get_char_cb) (cstate->app_cookie, gen);
		break;

	case CNTL_CMD_READ_LEAF:{
			dctl_subheader_t *shead;
			int32_t         opid;
			assert(data != NULL);

			shead = (dctl_subheader_t *) data;
			opid = ntohl(shead->dctl_opid);

			(*lstate->rleaf_cb) (cstate->app_cookie,
					     shead->dctl_data, opid);
			free(data);
			break;
		}

	case CNTL_CMD_WRITE_LEAF:{
			dctl_subheader_t *shead;
			int32_t         opid;
			int             dlen,
			                plen;
			assert(data != NULL);

			shead = (dctl_subheader_t *) data;
			opid = ntohl(shead->dctl_opid);
			dlen = ntohl(shead->dctl_dlen);
			plen = ntohl(shead->dctl_plen);

			(*lstate->wleaf_cb) (cstate->app_cookie,
					     shead->dctl_data, dlen,
					     &shead->dctl_data[plen], opid);

			free(data);
			break;
		}

	case CNTL_CMD_LIST_NODES:{
			dctl_subheader_t *shead;
			int32_t         opid;
			assert(data != NULL);

			shead = (dctl_subheader_t *) data;
			opid = ntohl(shead->dctl_opid);

			(*lstate->lnode_cb) (cstate->app_cookie,
					     shead->dctl_data, opid);
			free(data);
			break;
		}


	case CNTL_CMD_LIST_LEAFS:{
			dctl_subheader_t *shead;
			int32_t         opid;
			assert(data != NULL);

			shead = (dctl_subheader_t *) data;
			opid = ntohl(shead->dctl_opid);

			(*lstate->lleaf_cb) (cstate->app_cookie,
					     shead->dctl_data, opid);
			free(data);
			break;
		}


	case CNTL_CMD_SEND_OBJ:
		assert(data != NULL);
		process_object_message(lstate, cstate, data);
		free(data);
		cstate->pend_obj--;
		if ((cstate->pend_obj== 0) && (cstate->have_start)) {
			(*lstate->start_cb) (cstate->app_cookie, cstate->start_gen);
			cstate->have_start = 0;
		}
		break;

	case CNTL_CMD_SET_EXEC_MODE:
		{
		exec_mode_subheader_t *emheader;
		
		assert(data != NULL);
		emheader = (exec_mode_subheader_t *) data;
		(*lstate->set_exec_mode_cb) (cstate->app_cookie, emheader->mode);
		free(data);
		break;
		}
	case CNTL_CMD_SET_USER_STATE:
		{
		user_state_subheader_t *usheader;
		
		assert(data != NULL);
		usheader = (user_state_subheader_t *) data;
		(*lstate->set_user_state_cb) (cstate->app_cookie, usheader->state);
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
 * This reads data from the control socket and forwards it to the
 * TI-RPC server.
 */

void
sstub_read_control(listener_state_t * lstate, cstate_t * cstate)
{
	int size_in, size_out, error;
	char buf[4096];

	/*
	 * Handle the case where we are shutting down 
	 */
	if (cstate->flags & CSTATE_SHUTTING_DOWN) {
		printf("read control:  shutting down \n");
		return;
	}

	/* Attempt to process up to 4096 bytes of data. */
	
	size_in = read(cstate->control_fd, (void *)buf, 4096);
	if(size_in < 0){
	  perror("read");
	  return;
	}
	
	size_out = write(cstate->tirpc_fd, (void *)buf, size_in);
	if(size_out < 0) {
	  perror("write");
	  return;
	}
	
	if(size_in != size_out) {
	  fprintf(stderr, "Somehow lost bytes, from %d in to %d out!\n", 
		  size_in, size_out);
	  return;
	}

	return;
}
