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
#include "ring.h"
#include "lib_searchlet.h"
#include "attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_search_priv.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "lib_hstub.h"
#include "lib_dconfig.h"
#include "dctl_common.h"


#define	PROC_RING_SIZE		1024
#define	UNPROC_RING_SIZE	1024

/* XXX locking for multi-threaded apps !! */

/*
 * This funciton initilizes the search state and returns the handle
 * used for other calls.
 */

ls_search_handle_t
ls_init_search()
{
	search_context_t	*sc;
	int			err;

	sc = (search_context_t *)malloc(sizeof(*sc));
	if (sc == NULL) {
		/* XXX error log */
		return(NULL);
	}

	/*
	 * Initialize the logging on the local host.
	 */
	dctl_init();

	err = dctl_register_node(ROOT_PATH, HOST_PATH);
	assert(err == 0);

	err = dctl_register_node(ROOT_PATH, DEVICE_PATH);
	assert(err == 0);

	err = dctl_register_node(HOST_PATH, HOST_NETWORK_NODE);
	assert(err == 0);

	log_init();
	

	sc->cur_search_id = 1; /* XXX should we randomize ??? */
	sc->dev_list = NULL;
	sc->cur_status = SS_EMPTY;
	sc->bg_status = 0;
	err = ring_init(&sc->proc_ring, PROC_RING_SIZE);
	if (err) {
		/* XXX log */
		free(sc);
		return(NULL);
	}
	err = ring_init(&sc->unproc_ring, UNPROC_RING_SIZE);
	if (err) {
		/* XXX log */
		free(sc);
		return(NULL);
	}


	log_start(sc);
	dctl_start(sc);
	
	bg_init(sc, 1);

	return((ls_search_handle_t)sc);
}


/*
 * This stops the any current searches and releases the state
 * assciated with the search.
 */
int
ls_terminate_search(ls_search_handle_t handle)
{
	search_context_t	*sc;
	device_handle_t		*cur_dev;
	int			err;

	sc = (search_context_t *)handle;



	/*
	 * if there is a current search, we need to start shutting it down
	 */
	if (sc->cur_status == SS_ACTIVE) {
		cur_dev = sc->dev_list;

		while (cur_dev != NULL) {
			err = device_stop(cur_dev->dev_handle , 
					sc->cur_search_id);
			if (err != 0) {
				/* if we get an error we note it for
			 	 * the return value but try to process the rest
			 	 * of the devices
			 	 */
				/* XXX logging */
			}
			cur_dev = cur_dev->next;
		}
	}
	/* change the search id */
	sc->cur_search_id++;

	/* change to indicated we are shutting down */
	sc->cur_status = SS_SHUTDOWN;

	/*
	 * XXX think more about the shutdown.  How do we 
	 * make sure everything is done.
	 */

	/*
	 * Now we need to shutdown each of the device specific
	 * handlers.
	 */
	cur_dev = sc->dev_list;

	while (cur_dev != NULL) {
		err = device_terminate(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			/* 
			 * if we get an error we note it for
		 	 * the return value but try to process the rest
		 	 * of the devices
		 	 */
			/* XXX logging */
		}
		cur_dev = cur_dev->next;
	}

	return (0);	
}


#define	MAX_HOST_IDS	16

int
ls_set_searchlist(ls_search_handle_t handle, int num_groups,
		groupid_t *glist)
{
	search_context_t *	sc;
	groupid_t		cur_gid;
	device_handle_t		*cur_dev;
	uint32_t		host_ids[MAX_HOST_IDS];
	int			hosts;
	int			i,j;
	int			err;


	sc = (search_context_t *)handle;

	/*
	 * we have two steps.  One is to clear the current
   	 * searchlist on all the devices that
	 * we are currently connected to.  The to add the gid
     * to each of the devices.
     */
	/* XXX todo, clean up connection not involved in search
     * after this call.
     */

	/* clear the state */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		cur_dev->num_groups = 0;
		err = device_clear_gids(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			/* 
			 * if we get an error we note it for
		 	 * the return value but try to process the rest
		 	 * of the devices
		 	 */
			/* XXX logging */
		}
	}


	/*
	 * for each of the groups, get the list
	 * of machines that have some of this data.
	 */
	for (i=0; i < num_groups; i++) {
		cur_gid = glist[i];
		hosts = MAX_HOST_IDS;
		glkup_gid_hosts(cur_gid, &hosts, host_ids);
		for (j=0; j<hosts; j++) {
			err = device_add_gid(sc, cur_gid, host_ids[j]);
			if (err) {
                struct in_addr in;    
                char *  name;
				/*
				 * we failed to add of init with the host,
				 * just fail this call for now, this
				 * is basically a bad state
				 * we can't recover from.
				 */
                in.s_addr = host_ids[j];
                name = inet_ntoa(in);
				printf("Failed to connect to device %s for gid %llx\n", 
				 	name, cur_gid);
                assert(0);
				return (EINVAL);
			}
		}	
	}

	/* XXX push the list of groups to disk */

	return(0);
}






int
ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
		 char *filter_file_name, char *filter_spec_name)
{

	search_context_t	*sc;
	device_handle_t		*cur_dev;
	int			err;
	int			started = 0;;

	sc = (search_context_t *)handle;

	/* XXX do something with the isa_type !! */
	/*
	 * if state is active, we can't change the searchlet.
	 * XXX what other states are no valid ??
	 */
	if (sc->cur_status == SS_ACTIVE) {
		/* XXX log */
		printf("still idle \n");
		return (EINVAL);
	}

	/* we need to verify the searchlet somehow */
	cur_dev = sc->dev_list;
	while (cur_dev != NULL) {
		err = device_set_searchlet(cur_dev->dev_handle, 
			sc->cur_search_id, filter_file_name, filter_spec_name);
		if (err != 0) {
			/*
			 * It isn't obvious what we need to do if we
			 * get an error here.  This applies to the fault
			 * tolerance story.  For now we keep trying the
			 * rest of the device
			 * XXX figure out what to do here ???
			 */
			/* XXX logging */
		} else {
			started++;
		}
		cur_dev = cur_dev->next;
	}

	err = bg_set_searchlet(sc, sc->cur_search_id, 
			filter_file_name, filter_spec_name);
	if (err) {
		/* XXX log */
	}

	/* XXXX */
	sc->cur_status = SS_IDLE;

	return(0);
}


int
ls_set_device_searchlet(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
			device_isa_t isa_type,
			 char *filter_file_name, char *filter_spec_name)
{

	/* XXXX do this */
	return (0);
}


/*
 * This initites a search on the disk.  For this to work, 
 * we need to make sure that we have searchlets set for all
 * the devices and that we have an appropraite search_set.
 */
int
ls_start_search(ls_search_handle_t handle)
{

	search_context_t	*sc;
	device_handle_t		*cur_dev;
	int			err;
	int			started = 0;;

	sc = (search_context_t *)handle;

	/*
	 * if state isn't idle, then we haven't set the searchlet on
	 * all the devices, so this is an error condition.
	 *
	 * If the cur_status == ACTIVE, it is already running which
	 * is also an error.
	 *
	 */
	if (sc->cur_status != SS_IDLE) {
		/* XXX log */
		return (EINVAL);
	}

	cur_dev = sc->dev_list;
	while (cur_dev != NULL) {
		/* clear the complete flag */
		cur_dev->flags &= ~DEV_FLAG_COMPLETE;
		err = device_start(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			/*
			 * It isn't obvious what we need to do if we
			 * get an error here.  This applies to the fault
			 * tolerance story.  For now we keep trying the
			 * rest of the device
			 * XXX figure out what to do here ???
			 */
			assert(0);
			/* XXX logging */
		} else {
			started++;
		}
		cur_dev = cur_dev->next;
	}

	/* XXX */
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
	search_context_t	*sc;
	device_handle_t		*cur_dev;
	int			err;
	int			ret_err;

	sc = (search_context_t *)handle;

	/*
	 * If no search is currently active (or just completed) then
	 * this is an error.
	 */
	if ((sc->cur_status != SS_ACTIVE) && (sc->cur_status != SS_DONE)) {
		return (EINVAL);
	}

	cur_dev = sc->dev_list;

	ret_err = 0;

	while (cur_dev != NULL) {
		err = device_stop(cur_dev->dev_handle, sc->cur_search_id);
		if (err != 0) {
			/* if we get an error we note it for
			 * the return value but try to process the rest
			 * of the devices
			 */
			/* XXX logging */
			ret_err = EINVAL;
		}
		cur_dev = cur_dev->next;
	}

	/* change the search id */
	sc->cur_search_id++;

	/* change the current state to idle */
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
 * 	0          - The search aborted cleanly.
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
ls_next_object(ls_search_handle_t handle, ls_obj_handle_t *obj_handle,
		int flags)
{

	search_context_t	*sc;
	obj_data_t	*	obj_data;
	void *			data;
	struct timespec 	timeout;


	/* XXX  make sure search is running */ 

	sc = (search_context_t *)handle;


	/*
	 * Try to get an item from the queue, if data is not available,
	 * then we either spin or return EWOULDBLOCK based on the
	 * flags.
	 */
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
			return(EWOULDBLOCK); 
		}

		/* 
		 * We need to sleep until data is available, we do a
		 * timed sleep for now. 
		 */
		timeout.tv_sec = 0;
		timeout.tv_nsec = 30000000; 	/* 30 ms */
		nanosleep(&timeout, NULL);

	}
	obj_data = (obj_data_t *)data;

	/* XXX how should we get this state really ?? */
	obj_data->cur_offset = 0;
	obj_data->cur_blocksize = 1024;

	*obj_handle = (ls_obj_handle_t *)data;

	return (0);
}



 
/*
 * This call is performed by the application to release object it obtained 
 * through ls_next_object.  This will causes all object storage and 
 * assocaited  state to be freed.  It will also invalidate all object 
 * mappings obtained through ls_map_object().
 *
 * Args:
 * 	handle	   - the search handle returned by init_libsearchlet().
 *
 * 	obj_handle - the object handle.
 *
 * Return:
 * 	0	   - the search aborted cleanly.
 *
 * 	EINVAL     - one of the handles was invalid. 
 *
 *
 * For now are using malloc/free to handle the data, so we 
 * will try to remove them.
 */

int
ls_release_object(ls_search_handle_t handle, ls_obj_handle_t obj_handle)
{
	obj_data_t *	new_obj;

	new_obj = (obj_data_t *)obj_handle;

	if (new_obj->data != NULL) {
		free(new_obj->data);
	}
	if (new_obj->attr_info.attr_data != NULL) {
		free(new_obj->attr_info.attr_data);
	}
	free(new_obj);
	return(0);
}

/*
 * This gets a list of all the storage devices that will be involved in
 * the search.  The results are returned as an array of device handles.   
 *
 * Args:
 * 	handle	    - the search handle returned by init_libsearchlet().
 *
 * 	handle_list - A pointer to a caller allocated array of device handles.
 *
 * 	num_handles - A pointer to an integer.  The caller sets this value to
 * 		      indicate the space allocated in handle list.  On return,
 * 		      this value will hold the number of handles filled in.  
 * 		      If thecaller did not allocate sufficient space, then 
 * 		      ENOSPC will be returned and the num_handles will 
 * 		      indicate the space necessary for the call to succeed.
 *
 * Returns:
 * 	0  	    - the call was successful.
 *
 *	EINVAL      -  One of the handles is not valid.
 *
 * 	EBUSY       - a search is currently active.
 *
 * 	ENOSPC      - The caller did not provide enough storage (the value 
 * 		      stored at num_handles was too small).  In this case the 
 * 		      value stored at num_handles will be updated to 
 * 		      indicate the amount of space needed.
 *
 */

int
ls_get_dev_list(ls_search_handle_t handle, ls_dev_handle_t *handle_list,
		int *num_handles)
{
	search_context_t	*sc;
	device_handle_t		*cur_dev;
	int                      dev_count;

	if(!handle_list) return EINVAL;
	if(!num_handles) return EINVAL;

	sc = (search_context_t *)handle;
	/* XXX check for active? */
	cur_dev = sc->dev_list;
	
	dev_count = 0;
	while(cur_dev != NULL) {
		if(*num_handles <= dev_count) return ENOSPC;
		dev_count++;
		*handle_list = cur_dev;
		handle_list++;
		cur_dev = cur_dev->next;
	}
	*num_handles = dev_count;
	return 0;
}


/*
 * This call takes a specific device handle and returns the characteristics of
 * this device.  
 *
 * Args:
 * 	handle	    - the search handle returned by init_libsearchlet().
 *
 * 	dev_handle  - The handle for the device being queried.
 *
 * 	dev_char    - A pointer to the location where the device 
 * 		      charactersitics should be stored.
 *
 * Returns:
 *	0	    - Call succeeded.
 *
 *	EINVAL      -  One of the handles is not valid.
 * 	
 *
 */

int
ls_dev_characteristics(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
		       device_char_t *dev_chars)
{
	device_handle_t *dev;

	dev = (device_handle_t *)dev_handle;
	/* validate dev? */

	return device_characteristics(dev->dev_handle, dev_chars);
}




/*
 * This call gets the current statistics from device specified by the device
 * handle.  This includes statistics on the device as well as any currently 
 * running search.
 * 
 * Args:
 * 	handle         - The handle for the search instance.
 *
 * 	dev_handle     - The handle for the device being queried.
 *
 * 	dev_stats      - This is the location where the device statistics should
 * 			 be stored.  This is allocated by the caller.
 * 
 * 	stat_len       - A pointer to an integer.  The caller sets this value to
 * 			 the amount of space allocated for the statistics.  Upon
 * 			 return, the call will set this to the amount of 
 * 			 space used.  If the call failed because of 
 * 			 insufficient space, ENOSPC, the call the will set 
 * 			 this value to the amount of space needed.
 *
 * Returns:
 * 	0              - The call completed successfully.
 *
 * 	ENOSPC	       - The caller did not allocated sufficient space for 
 * 			 the results.
 *
 * 	EINVAL	       - Either the search handle or the device handle are 
 * 			 invalid.
 *
 */

int
ls_get_dev_stats(ls_search_handle_t handle, ls_dev_handle_t  dev_handle,
		 dev_stats_t *dev_stats, int *stat_len)
{
	device_handle_t *dev;
	dev = (device_handle_t *)dev_handle;

	return device_statistics(dev->dev_handle, dev_stats, stat_len);
}
