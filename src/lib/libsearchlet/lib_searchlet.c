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
#include <sys/time.h>
#include "ring.h"
#include "lib_searchlet.h"
#include "obj_attr.h"
#include "lib_search_priv.h"
#include "lib_log.h"


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

	
	/* XXX */
	log_message(1, 1, "012345");

	sc->cur_search_id = 1; /* XXX should we randomize ??? */
	sc->dev_list = NULL;
	sc->cur_state = SS_EMPTY;
	err = ring_init(&sc->proc_ring);
	if (err) {
		/* XXX log */
		free(sc);
		return(NULL);
	}
	err = ring_init(&sc->unproc_ring);
	if (err) {
		/* XXX log */
		free(sc);
		return(NULL);
	}
	
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
	device_state_t		*cur_dev;
	int			err;

	sc = (search_context_t *)handle;



	/*
	 * if there is a current search, we need to start shutting it down
	 */
	if (sc->cur_state == SS_ACTIVE) {
		cur_dev = sc->dev_list;

		while (cur_dev != NULL) {
			err = device_stop(cur_dev, sc->cur_search_id);
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
	sc->cur_state = SS_SHUTDOWN;

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
		err = device_terminate(cur_dev, sc->cur_search_id);
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



int
ls_set_searchlist(ls_search_handle_t handle)
{
	search_context_t	*sc;
	device_state_t *new_dev;

	sc = (search_context_t *)handle;
/*
XXX do this
*/
	/* we actually want to craete a new device from
	 * all items added to search list.  Fix this later !!!
	 * XXXXXXXXXXX
	 */
	new_dev = device_init(sc, 1);
	if (new_dev == NULL) {
		/* XXX log */
		return (EINVAL);
	}

	/*
	 * Put this device on the list of devices involved
	 * in the search.
	 */
	new_dev->next = sc->dev_list;
	sc->dev_list = new_dev;

	return(0);
}






int
ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
		 char *filter_file_name, char *filter_spec_name)
{

	search_context_t	*sc;
	device_state_t		*cur_dev;
	int			err;
	int			started = 0;;

	sc = (search_context_t *)handle;

	/* XXX do something with the isa_type !! */
	/*
	 * if state is active, we can't change the searchlet.
	 * XXX what other states are no valid ??
	 */
	if (sc->cur_state == SS_ACTIVE) {
		/* XXX log */
		printf("still idle \n");
		return (EINVAL);
	}

	/* we need to verify the searchlet somehow */
	cur_dev = sc->dev_list;
	while (cur_dev != NULL) {
		err = device_set_searchlet(cur_dev, sc->cur_search_id, 
			filter_file_name, filter_spec_name);
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
	printf("set searchlet !! \n");
	sc->cur_state = SS_IDLE;

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
	device_state_t		*cur_dev;
	int			err;
	int			started = 0;;

	sc = (search_context_t *)handle;

	/*
	 * if state isn't idle, then we haven't set the searchlet on
	 * all the devices, so this is an error condition.
	 *
	 * If the cur_state == ACTIVE, it is already running which
	 * is also an error.
	 *
	 */
	if (sc->cur_state != SS_IDLE) {
		/* XXX log */
		printf("still idle \n");
		return (EINVAL);
	}

	cur_dev = sc->dev_list;
	while (cur_dev != NULL) {
		err = device_start(cur_dev, sc->cur_search_id);
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

	/* XXX */
	started++;

	if (started > 0) {
		sc->cur_state = SS_ACTIVE;
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
	device_state_t		*cur_dev;
	int			err;
	int			ret_err;

	sc = (search_context_t *)handle;

	/*
	 * If no search is currently active (or just completed) then
	 * this is an error.
	 */
	if ((sc->cur_state != SS_ACTIVE) && (sc->cur_state != SS_DONE)) {
		return (EINVAL);
	}

	cur_dev = sc->dev_list;

	ret_err = 0;

	while (cur_dev != NULL) {
		err = device_stop(cur_dev, sc->cur_search_id);
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
	sc->cur_state = SS_IDLE;
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
	pthread_mutex_t 	mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t  	cond = PTHREAD_COND_INITIALIZER;
	struct timeval 		now;
	struct timespec 	timeout;
	struct timezone 	tz;


	/* XXX  make sure search is running */ 

	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;

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

		if (sc->cur_state == SS_DONE) {
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
		pthread_mutex_lock(&mut);
		gettimeofday(&now, &tz);
		timeout.tv_sec = now.tv_sec + 1;
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&cond, &mut, &timeout);
		pthread_mutex_unlock(&mut);

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

