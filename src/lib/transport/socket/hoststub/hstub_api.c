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
#include "ring.h"
#include "lib_searchlet.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_hstub.h"
#include "hstub_impl.h"


/*
 * Return the cache device characteristics.
 */
int
device_characteristics(void *handle, device_char_t *dev_chars)
{
	sdevice_state_t *dev = (sdevice_state_t *)handle;

	*dev_chars = dev->dev_char; 

	/* XXX debug */
	assert(dev_chars->dc_isa == dev->dev_char.dc_isa);
	assert(dev_chars->dc_speed == dev->dev_char.dc_speed);
	assert(dev_chars->dc_mem == dev->dev_char.dc_mem);

	return (0);
}


/*
 * This is the entry point to stop a current search.  This build the control
 * header and places it on a queue to be transmitted to the devce.
 */ 

int
device_stop(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;


	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_STOP);
	cheader->data_len = htonl(0);

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed device stop \n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);

	return (0);
}

/*
 * Terminate an ongoing search.  This sends out a termninate request.
 * Once the remote side has finished, then it will send a TERM_DONE
 * request to finish.
 */

int
device_terminate(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;



	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_TERMINATE);
	cheader->data_len = htonl(0);

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		free(cheader);
		return (EAGAIN);
	}
	/* XXX flags */
	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}


/*
 * This start a search that has been setup.  
 */


int
device_start(void *handle, int id)
{
	int			err;
	control_header_t *	cheader;
	sdevice_state_t *dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_START);
	cheader->data_len = htonl(0);

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		printf("XXX failed to enq start \n");
		free(cheader);
		return (EAGAIN);
	}

	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}


/*
 * This builds the command to set the searchlet on the remote device.
 * This builds the buffers and copies the contents of the files into
 * the buffers.
 */
 

int
device_set_searchlet(void *handle, int id, char *filter, char *spec)
{
	int			err;
	control_header_t *	cheader;
	searchlet_subhead_t *	shead;
	char *			data;
	int			total_len;
	int			spec_len;
	int			filter_len;
	struct stat 		stats;
	ssize_t			rsize;
	FILE *			cur_file;
	sdevice_state_t *	dev;

	dev = (sdevice_state_t *)handle;

	cheader = (control_header_t *) malloc(sizeof(*cheader));	
	if (cheader == NULL) {
		/* XXX log */
		return (EAGAIN);
	}

	cheader->generation_number = htonl(id);
	cheader->command = htonl(CNTL_CMD_SET_SEARCHLET);


	/*
	 * Now get the total size of the filter spec and the filter
	 */
	err = stat(filter, &stats);
	if (err) {
		/* XXX log */
		return (ENOENT);
	}
	filter_len = stats.st_size;

	err = stat(spec, &stats);
	if (err) {
		/* XXX log */
		return (ENOENT);
	}
	spec_len = stats.st_size;


	total_len = ((spec_len + 3) & ~3) + filter_len + sizeof(*shead);
	cheader->data_len = htonl(total_len);

	shead = (searchlet_subhead_t *) malloc(total_len);
	if (shead == NULL) {
		free(cheader);
		return (EAGAIN);
	}

	shead->spec_len = htonl(spec_len);
	shead->filter_len = htonl(filter_len);
	

	/* 
	 * set data to the beginning of the data portion  and
	 * copy in the filter spec from the file.  NOTE: This is
	 * currently blocks, we may want to do something else later.
	 */
	data = (char *)shead + sizeof(*shead);
	cur_file = fopen(spec, "r");
	if (cur_file == NULL) {
		/* XXX log */
		free(cheader);
		free(shead);
		return (ENOENT);
	}
	rsize = fread(data, spec_len, 1, cur_file);
	if (rsize != 1) {
		/* XXX log */
		free(cheader);
		free(shead);
		return(EAGAIN);
	}

	fclose(cur_file);


	/*
	 * Now set data to where we are going to store the searchlet.
	 * Data is the size of the spec_len rounded up to the next 4-byte
	 * boundary.
	 */
	data += ((spec_len + 3) & ~3);
	cur_file = fopen(filter, "r");
	if (cur_file == NULL) {
		/* XXX log */
		free(cheader);
		free(shead);
		return (ENOENT);
	}
	rsize = fread(data, filter_len, 1, cur_file);
	if (rsize != 1) {
		/* XXX log */
		free(cheader);
		free(shead);
		return(EAGAIN);
	}

	fclose(cur_file);


	/*
	 * XXX HACK.  For now we store the pointer to the data
	 * using the spare field in the header to point to the data.
	 */
	cheader->spare = (uint32_t) shead;	

	err = ring_enq(dev->device_ops, (void *)cheader);
	if (err) {
		/* XXX log */
		/* XXX should we wait ?? */
		free(cheader);
		return (EAGAIN);
	}
	pthread_mutex_lock(&dev->con_data.mutex);
	dev->con_data.flags |= CINFO_PENDING_CONTROL;
	pthread_mutex_unlock(&dev->con_data.mutex);
	return (0);
}


/*
 * This is the initialization function that is
 * called by the searchlet library when we startup.
 */

/* XXX callback for new packets  */
void *
device_init(int id, char * devid, void *hcookie, hstub_new_obj_fn new_obj_cb)
{
	sdevice_state_t *new_dev;
	int		err;

	new_dev = (sdevice_state_t *) malloc(sizeof(*new_dev));	
	if (new_dev == NULL) {
		return (NULL);
	}

	/*
	 * initialize the ring that is used to queue "commands"
	 * to the background thread.
	 */
	err = ring_init(&new_dev->device_ops);
	if (err) {
		free(new_dev);
		return (NULL);
	}

	new_dev->flags = 0;

	pthread_mutex_init(&new_dev->con_data.mutex, NULL);

	/*
	 * Open the sockets to the new host.
	 */
	err = hstub_establish_connection(&new_dev->con_data, devid);
	if (err) {
		/* XXX log,  */
		free(new_dev);
		return(NULL);
	}

	/*
	 * Save the callback and the host cookie.
	 */
	new_dev->hcookie = hcookie;
	new_dev->hstub_new_obj_cb = new_obj_cb;	


	/*
	 * Spawn a thread for this device that process data to and
	 * from the device.
	 */

	err = pthread_create(&new_dev->thread_id, NULL, hstub_main, 
			    (void *)new_dev);
	if (err) {
		/* XXX log */
		free(new_dev);
		return (NULL);
	}


	return((void *)new_dev);
}


/*
 * This is used to tear down the state assocaited with the
 * device search.
 */
void
device_fini(sdevice_state_t *dev_state)
{

	free(dev_state); 
}

