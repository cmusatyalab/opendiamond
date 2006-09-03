/*
 *      Diamond (Release 1.0)
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
 * This provides many of the main functions in the provided
 * through the searchlet API.
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "lib_tools.h"
#include "lib_searchlet.h"
#include "sys_attr.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_dconfig.h"
#include "dctl_common.h"


static char const cvsid[] =
    "$Header$";

#define	PROC_RING_SIZE		1024
#define	UNPROC_RING_SIZE	1024

/*
 * XXX locking for multi-threaded apps !! 
 */

/*
 * this is a helper function that is called before every
 * API call.  We need to make sure that the thread local
 * state is initialized correctly since we don't know anything
 * about how the application is calling us. 
 *
 * XXX it may be overkill to do this for every call, but
 * just the ones that touch state the uses the thread
 * specific data. 
 */

static void
thread_setup(search_context_t * sc)
{
	log_thread_register(sc->log_cookie);
	dctl_thread_register(sc->dctl_cookie);
}

/*
 * This funciton initilizes the search state and returns the handle
 * used for other calls.
 */

ls_search_handle_t
ls_init_search()
{
	search_context_t *sc;
	int             err;

	sc = (search_context_t *) malloc(sizeof(*sc));
	if (sc == NULL) {
		/*
		 * XXX error log 
		 */
		return (NULL);
	}

	/*
	 * Initialize the logging on the local host.
	 */
	err = dctl_init(&sc->dctl_cookie);
	assert(err == 0);

	err = dctl_register_node(ROOT_PATH, HOST_PATH);
	assert(err == 0);

	err = dctl_register_node(ROOT_PATH, DEVICE_PATH);
	assert(err == 0);

	err = dctl_register_node(HOST_PATH, HOST_NETWORK_NODE);
	assert(err == 0);

	log_init(&sc->log_cookie);

	sc->cur_search_id = 1;	/* XXX should we randomize ??? */
	sc->dev_list = NULL;
	sc->cur_status = SS_EMPTY;
	sc->bg_status = 0;
	sc->pend_hw = LS_OBJ_PEND_HW;
	sc->pend_lw = LS_OBJ_PEND_LW;
	sc->last_dev = NULL;
	sc->bg_credit_policy = BG_DEFAULT_CREDIT_POLICY;
	err = ring_init(&sc->proc_ring, PROC_RING_SIZE);
	if (err) {
		/*
		 * XXX log 
		 */
		free(sc);
		return (NULL);
	}


	sig_cal_init();

	log_start(sc);
	dctl_start(sc);

	bg_init(sc, 1);

	return ((ls_search_handle_t) sc);
}


/*
 * This stops the any current searches and releases the state
 * assciated with the search.
 */
int
ls_terminate_search(ls_search_handle_t handle)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;

	sc = (search_context_t *) handle;

	thread_setup(sc);


	/*
	 * if there is a current search, we need to start shutting it down
	 */
	if (sc->cur_status == SS_ACTIVE) {
		for (cur_dev = sc->dev_list; cur_dev != NULL; 
		    cur_dev = cur_dev->next) {
			if (cur_dev->flags & DEV_FLAG_DOWN) {
				continue;
			}
			err = device_stop(cur_dev->dev_handle,
					  sc->cur_search_id);
			if (err != 0) {
				/*
				 * if we get an error we note it for * the
				 * return value but try to process the rest * 
				 * of the devices 
				 */
				/*
				 * XXX logging 
				 */
			}
		}
	}
	/*
	 * change the search id 
	 */
	sc->cur_search_id++;

	/*
	 * change to indicated we are shutting down 
	 */
	sc->cur_status = SS_SHUTDOWN;

	/*
	 * XXX think more about the shutdown.  How do we 
	 * make sure everything is done.
	 */

	/*
	 * Now we need to shutdown each of the device specific
	 * handlers.
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_terminate(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			/*
			 * if we get an error we note it for
			 * the return value but try to process the rest
			 * of the devices
			 */
			/*
			 * XXX logging 
			 */
		}
	}

	return (0);
}


/*
 * simpling error logging for failed device.
 */

static void
log_dev_error(uint32_t host, const char *str)
{
	struct in_addr  in;
	char           *name;

	in.s_addr = host;
	name = inet_ntoa(in);

	log_message(LOGT_BG, LOGL_CRIT, "%s for device %s", str, name);
}


#define	MAX_HOST_IDS	64

int
ls_set_searchlist(ls_search_handle_t handle, int num_groups,
		  groupid_t * glist)
{
	search_context_t *sc;
	groupid_t       cur_gid;
	device_handle_t *cur_dev;
	uint32_t        host_ids[MAX_HOST_IDS];
	int             hosts;
	int             i,
	                j;
	int             err;

	sc = (search_context_t *) handle;
	thread_setup(sc);


	/*
	 * we have two steps.  One is to clear the current
	 * searchlist on all the devices that
	 * we are currently connected to.  The to add the gid
	 * to each of the devices.
	 */
	/*
	 * XXX todo, clean up connection not involved in search after this
	 * call. 
	 */

	/*
	 * clear the state 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		cur_dev->num_groups = 0;
		err = device_clear_gids(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			log_dev_error(cur_dev->dev_id, "failed clear gid");
		}
	}


	/*
	 * for each of the groups, get the list
	 * of machines that have some of this data.
	 */
	for (i = 0; i < num_groups; i++) {
		cur_gid = glist[i];
		hosts = MAX_HOST_IDS;
		glkup_gid_hosts(cur_gid, &hosts, host_ids);
		for (j = 0; j < hosts; j++) {
			err = device_add_gid(sc, cur_gid, host_ids[j]);
			if (err) {
				log_dev_error(host_ids[j], "Failed to add gid");
			}
		}
	}

	/*
	 * XXX push the list of groups to disk 
	 */

	return (0);
}






int
ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
		 char *filter_file_name, char *filter_spec_name)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;
	int             started = 0;
	;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	/*
	 * XXX do something with the isa_type !! 
	 */
	/*
	 * if state is active, we can't change the searchlet.
	 * XXX what other states are no valid ??
	 */
	if (sc->cur_status == SS_ACTIVE) {
		/*
		 * XXX log 
		 */
		printf("still idle \n");
		return (EINVAL);
	}
	/*
	 * change the search id 
	 */
	sc->cur_search_id++;

	/*
	 * we need to verify the searchlet somehow 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_searchlet(cur_dev->dev_handle,
					   sc->cur_search_id,
					   filter_file_name,
					   filter_spec_name);
		if (err != 0) {
			log_dev_error(cur_dev->dev_id, 
			    "failed setting searchlet");
		} else {
			started++;
		}
	}

	err = bg_set_searchlet(sc, sc->cur_search_id,
       		filter_file_name, filter_spec_name);
	if (err) {
		/*
		 * XXX log 
		 */
		assert(0);
	}

	/*
	 * XXXX 
	 */
	sc->cur_status = SS_IDLE;

	return (0);
}

int
ls_add_filter_file(ls_search_handle_t handle, device_isa_t isa_type,
		   char *filter_file_name)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;
	int             started = 0;
	;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	/*
	 * XXX do something with the isa_type !! 
	 */
	/*
	 * if state is active, we can't change the searchlet.
	 * XXX what other states are no valid ??
	 */
	if (sc->cur_status == SS_ACTIVE) {
		/*
		 * XXX log 
		 */
		printf("still idle \n");
		return (EINVAL);
	}

	/*
	 * we need to verify the searchlet somehow 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_searchlet(cur_dev->dev_handle,
					   sc->cur_search_id,
					   filter_file_name, NULL);
		if (err != 0) {
			log_dev_error(cur_dev->dev_id, 
			    "failed adding filter file");
		} else {
			started++;
		}
	}

	err = bg_set_searchlet(sc, sc->cur_search_id, filter_file_name, NULL);
	if (err) {
		/*
		 * XXX log 
		 */
	}

	/*
	 * XXXX 
	 */
	sc->cur_status = SS_IDLE;

	return (0);
}


int
ls_set_device_searchlet(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
			device_isa_t isa_type,
			char *filter_file_name, char *filter_spec_name)
{

	/*
	 * XXXX do this 
	 */
	return (0);
}




/*
 * This call sets a "blob" of data to be passed to a given
 * filter.  This is a way to pass a large amount of data.
 *
 * This call should be called after the searchlet has been
 * loaded but before a search has begun.
 *
 * NOTE:  It is up to the caller to make sure this data
 * can be interpreted by at the device (endian issues, etc).
 *
 * Args:
 *      handle          -       The handle for the search instance.
 *
 *  filter_name         -       The name of the filter to use for the blob.
 *
 *  blob_len            -       The length of the blob data.
 *
 *  blob_data           -       A pointer to the blob data.
 * 
 *
 * Returns:
 *      0                - The call suceeded.
 *
 *      EINVAL           - One of the file names was invalid or 
 *                         one of the files could not be parsed.
 *
 *      EBUSY                    - A search was already active.
 */

int
ls_set_blob(ls_search_handle_t handle, char *filter_name,
	    int blob_len, void *blob_data)
{

	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	if (sc->cur_status == SS_ACTIVE) {
		/*
		 * XXX log 
		 */
		fprintf(stderr, " Search is active \n");
		return (EBUSY);
	}

	/*
	 * we need to verify the searchlet somehow 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_blob(cur_dev->dev_handle,
				      sc->cur_search_id, filter_name,
				      blob_len, blob_data);
		if (err != 0) {
			log_dev_error(cur_dev->dev_id, "failed to set blob");
		}
	}

	err = bg_set_blob(sc, sc->cur_search_id, filter_name, blob_len,
			  blob_data);
	if (err) {
		/*
		 * XXX log 
		 */
	}

	return (0);
}


/*
 * This call sets a "blob" of data to be passed to a given
 * filter on a specific device.  This is similiar to the above
 * call but will only affect one device instead of all devices.
 *
 * This call should be called after the searchlet has been
 * loaded but before a search has begun.
 *
 * NOTE:  It is up to the caller to make sure this data
 * can be interpreted by at the device (endian issues, etc).
 *
 * Args:
 *      handle          -       The handle for the search instance.
 *
 *      dev_handle       - The handle for the device.
 *
 *  filter_name         -       The name of the filter to use for the blob.
 *
 *  blob_len            -       The length of the blob data.
 *
 *  blob_data           -       A pointer to the blob data.
 * 
 *
 * Returns:
 *      0                - The call suceeded.
 *
 *      EINVAL           - One of the file names was invalid or 
 *                         one of the files could not be parsed.
 *
 *      EBUSY                    - A search was already active.
 */

int
ls_set_device_blob(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
		   char *filter_name, int blob_len, void *blob_data)
{

	/*
	 * XXXX implement 
	 */
	assert(0);
}



/*
 * This initites a search on the disk.  For this to work, 
 * we need to make sure that we have searchlets set for all
 * the devices and that we have an appropraite search_set.
 */
int
ls_start_search(ls_search_handle_t handle)
{

	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;
	int             started = 0;
	time_t          cur_time;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	/*
	 * if state isn't idle, then we haven't set the searchlet on
	 * all the devices, so this is an error condition.
	 *
	 * If the cur_status == ACTIVE, it is already running which
	 * is also an error.
	 *
	 */
	if (sc->cur_status != SS_IDLE) {
		/*
		 * XXX log 
		 */
		return (EINVAL);
	}

	err = bg_start_search(sc, sc->cur_search_id);
	if (err) {
		/*
		 * XXX log 
		 */
	}

	time(&cur_time);

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		/*
		 * clear the complete flag 
		 */
		cur_dev->flags &= ~DEV_FLAG_COMPLETE;
		cur_dev->start_time = cur_time;
		err = device_start(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			log_dev_error(cur_dev->dev_id,
			    "failed starting search");
		} else {
			started++;
		}
	}

	/*
	 * XXX 
	 */
	started++;

	if (started > 0) {
		sc->cur_status = SS_ACTIVE;
		return (0);
	} else {
		return (EINVAL);
	}
}


/*
 * This stops the current search.  We actually do an async shutdown
 * by changing the sequence number (so that we will just drop incoming
 * data) and asynchronously sending messages to the disk to stop processing.
 */

int
ls_abort_search(ls_search_handle_t handle)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;
	int             ret_err;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	/*
	 * If no search is currently active (or just completed) then
	 * this is an error.
	 */
	if ((sc->cur_status != SS_ACTIVE) && (sc->cur_status != SS_DONE)) {
		return (EINVAL);
	}

	err = bg_stop_search(sc, sc->cur_search_id);
	if (err) {
		/*
		 * XXX log 
		 */
	}

	ret_err = 0;
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_stop(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			/*
			 * if we get an error we note it for the return value 
			 * but try to process the rest of the devices 
			 */
			/*
			 * XXX logging 
			 */
			ret_err = EINVAL;
		}
	}

	/*
	 * change the search id 
	 */
	sc->cur_search_id++;

	/*
	 * change the current state to idle 
	 */
	sc->cur_status = SS_IDLE;
	return (ret_err);

}



/*
 * This call gets the next object that matches the searchlet.  The flags specify
 * the behavior for blocking.  If no flags are passed, then the call will block
 * until the next object is available or the search has completed.  If the flag
 * LSEARCH_NO_BLOCK is set, and no objects are currently available, then
 * the error EWOULDBLOCK is returned.
 *
 * Args:
 *      handle     - the search handle returned by init_libsearchlet().
 *
 *      obj_handle - a pointer to the location where the new object handle will
 *                    stored upon succesful completion of the call.
 * 
 * Return:
 *      0          - The search aborted cleanly.
 * 
 *      EINVAL     - There was no active search or the handle is invalid.
 *      
 *      EWOULDBLOCK - There are no objects currently available.
 *      
 *      ENOENT     - The search has been completed and all objects have
 *                   been searched.
 * 
 *
 */

int
ls_next_object(ls_search_handle_t handle, ls_obj_handle_t * obj_handle,
	       int flags)
{
	search_context_t *sc;
	obj_data_t     *obj_data;
	obj_info_t     *obj_info;
	void           *data;
	struct timespec timeout;


	/*
	 * XXX make sure search is running 
	 */
	sc = (search_context_t *) handle;
	thread_setup(sc);


	/*
	 * Try to get an item from the queue, if data is not available,
	 * then we either spin or return EWOULDBLOCK based on the
	 * flags.
	 */
      again:
	while ((data = ring_deq(sc->proc_ring)) == NULL) {

		/*
		 * Make sure we are still processing data.
		 */

		if (sc->cur_status == SS_DONE) {
			return (ENOENT);
		}


		/*
		 * See if we are blocking or non-blocking.
		 */
		if ((flags & LSEARCH_NO_BLOCK) == LSEARCH_NO_BLOCK) {
			return (EWOULDBLOCK);
		}

		/*
		 * We need to sleep until data is available, we do a
		 * timed sleep for now. 
		 */
		timeout.tv_sec = 0;
		timeout.tv_nsec = 10000000;	/* 10 ms */
		nanosleep(&timeout, NULL);
	}
	obj_info = (obj_info_t *) data;
	obj_data = obj_info->obj;
	if (sc->cur_search_id != obj_info->ver_num) {
		ls_release_object(sc, obj_data);
		free(obj_info);
		goto again;
	}

	free(obj_info);

	/*
	 * XXX how should we get this state really ?? 
	 */
	obj_data->cur_offset = 0;
	obj_data->cur_blocksize = 1024;

	*obj_handle = (ls_obj_handle_t *) obj_data;
	return (0);
}

int
ls_num_objects(ls_search_handle_t handle, int *obj_cnt)
{

	search_context_t *sc;

	/*
	 * XXX make sure search is running 
	 */

	sc = (search_context_t *) handle;
	thread_setup(sc);

	*obj_cnt = ring_count(sc->proc_ring);

	return (0);
}



/*
 * This call is performed by the application to release object it obtained 
 * through ls_next_object.  This will causes all object storage and 
 * assocaited  state to be freed.  It will also invalidate all object 
 * mappings obtained through ls_map_object().
 *
 * Args:
 *      handle     - the search handle returned by init_libsearchlet().
 *
 *      obj_handle - the object handle.
 *
 * Return:
 *      0          - the search aborted cleanly.
 *
 *      EINVAL     - one of the handles was invalid. 
 *
 *
 * For now are using malloc/free to handle the data, so we 
 * will try to remove them.
 */

int
ls_release_object(ls_search_handle_t handle, ls_obj_handle_t obj_handle)
{
	obj_data_t     *new_obj;
	obj_adata_t    *cur,
	               *next;

	new_obj = (obj_data_t *) obj_handle;

	if (new_obj->base != NULL) {
		free(new_obj->base);
	}

	cur = new_obj->attr_info.attr_dlist;
	while (cur != NULL) {
		next = cur->adata_next;
		free(cur->adata_base);
		free(cur);
		cur = next;
	}
	free(new_obj);
	return (0);
}

/*
 * This gets a list of all the storage devices that will be involved in
 * the search.  The results are returned as an array of device handles.   
 *
 * Args:
 *      handle      - the search handle returned by init_libsearchlet().
 *
 *      handle_list - A pointer to a caller allocated array of device handles.
 *
 *      num_handles - A pointer to an integer.  The caller sets this value to
 *                    indicate the space allocated in handle list.  On return,
 *                    this value will hold the number of handles filled in.  
 *                    If thecaller did not allocate sufficient space, then 
 *                    ENOSPC will be returned and the num_handles will 
 *                    indicate the space necessary for the call to succeed.
 *
 * Returns:
 *      0           - the call was successful.
 *
 *      EINVAL      -  One of the handles is not valid.
 *
 *      EBUSY       - a search is currently active.
 *
 *      ENOSPC      - The caller did not provide enough storage (the value 
 *                    stored at num_handles was too small).  In this case the 
 *                    value stored at num_handles will be updated to 
 *                    indicate the amount of space needed.
 *
 */

int
ls_get_dev_list(ls_search_handle_t handle, ls_dev_handle_t * handle_list,
		int *num_handles)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             dev_count;

	if (!handle_list)
		return EINVAL;
	if (!num_handles)
		return EINVAL;

	sc = (search_context_t *) handle;
	thread_setup(sc);
	/*
	 * XXX check for active? 
	 */
	dev_count = 0;
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		if (*num_handles <= dev_count)
			return ENOSPC;
		dev_count++;
		*handle_list = cur_dev;
		handle_list++;
	}
	*num_handles = dev_count;
	return 0;
}


/*
 * This call takes a specific device handle and returns the characteristics of
 * this device.  
 *
 * Args:
 *      handle      - the search handle returned by init_libsearchlet().
 *
 *      dev_handle  - The handle for the device being queried.
 *
 *      dev_char    - A pointer to the location where the device 
 *                    charactersitics should be stored.
 *
 * Returns:
 *      0           - Call succeeded.
 *
 *      EINVAL      -  One of the handles is not valid.
 *      
 *
 */

int
ls_dev_characteristics(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
		       device_char_t * dev_chars)
{
	device_handle_t *dev;
	search_context_t *sc;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	dev = (device_handle_t *) dev_handle;
	/*
	 * validate dev? 
	 */

	return device_characteristics(dev->dev_handle, dev_chars);
}




/*
 * This call gets the current statistics from device specified by the device
 * handle.  This includes statistics on the device as well as any currently 
 * running search.
 * 
 * Args:
 *      handle         - The handle for the search instance.
 *
 *      dev_handle     - The handle for the device being queried.
 *
 *      dev_stats      - This is the location where the device statistics should
 *                       be stored.  This is allocated by the caller.
 * 
 *      stat_len       - A pointer to an integer.  The caller sets this value to
 *                       the amount of space allocated for the statistics.  Upon
 *                       return, the call will set this to the amount of 
 *                       space used.  If the call failed because of 
 *                       insufficient space, ENOSPC, the call the will set 
 *                       this value to the amount of space needed.
 *
 * Returns:
 *      0              - The call completed successfully.
 *
 *      ENOSPC         - The caller did not allocated sufficient space for 
 *                       the results.
 *
 *      EINVAL         - Either the search handle or the device handle are 
 *                       invalid.
 *
 */

int
ls_get_dev_stats(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
		 dev_stats_t * dev_stats, int *stat_len)
{
	device_handle_t *dev;
	search_context_t *sc;

	sc = (search_context_t *) handle;
	thread_setup(sc);

	dev = (device_handle_t *) dev_handle;

	/*
	 * check that this is a valid argument 
	 */
	if (dev == NULL) {
		return (EINVAL);
	}

	return device_statistics(dev->dev_handle, dev_stats, stat_len);
}
