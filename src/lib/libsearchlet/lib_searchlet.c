/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  All rights reserved.
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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include <string.h>
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
#include "lib_filterexec.h"


#define	PROC_RING_SIZE		1024
#define	UNPROC_RING_SIZE	1024
#define LOG_PREFIX			"diamond_client"

/*
 * XXX locking for multi-threaded apps !! 
 */

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
	err = dctl_register_node(ROOT_PATH, HOST_PATH);
	assert(err == 0);

	err = dctl_register_node(ROOT_PATH, DEVICE_PATH);
	assert(err == 0);

	err = dctl_register_node(HOST_PATH, HOST_NETWORK_NODE);
	assert(err == 0);

	log_init(LOG_PREFIX, ROOT_PATH);

	sc->Xcur_search_id = 1;	/* XXX should we randomize ??? */
	sc->dev_list = NULL;
	sc->cur_status = SS_EMPTY;
	sc->bg_status = 0;
	sc->pend_lw = LS_OBJ_PEND_LW;
	sc->dev_queue_limit = DEFAULT_QUEUE_LEN;
	sc->last_dev = NULL;
	sc->bg_credit_policy = BG_DEFAULT_CREDIT_POLICY;
	sc->search_exec_mode = FM_CURRENT;
	err = ring_init(&sc->proc_ring, PROC_RING_SIZE);
	if (err) {
		/*
		 * XXX log 
		 */
		free(sc);
		return (NULL);
	}

	dctl_start(sc);

	bg_init(sc);

	log_message(LOGT_BG, LOGL_TRACE, "ls_init_search");
	
	return ((ls_search_handle_t) sc);
}


/*
 * This stops the any current searches and releases the state
 * assciated with the search.
 */
int
ls_terminate_search(ls_search_handle_t handle)
{
  app_stats_t astats;

  memset(&astats, 0, sizeof(app_stats_t));
  return (ls_terminate_search_extended(handle, &astats));
}

int
ls_terminate_search_extended(ls_search_handle_t handle, app_stats_t *as)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;

	sc = (search_context_t *) handle;

	log_message(LOGT_BG, LOGL_TRACE, "ls_terminate_search");

	/*
	 * if there is a current search, we need to start shutting it down
	 */
	if (sc->cur_status == SS_ACTIVE) {
		for (cur_dev = sc->dev_list; cur_dev != NULL; 
		    cur_dev = cur_dev->next) {
			if (cur_dev->flags & DEV_FLAG_DOWN) {
				continue;
			}
			sc->host_stats.hs_objs_uqueued = as->as_objs_queued;
			sc->host_stats.hs_objs_upresented = as->as_objs_presented;
			err = device_stop(cur_dev->dev_handle, &sc->host_stats);
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
	sc->Xcur_search_id++;

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
		err = device_terminate(cur_dev->dev_handle);
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


/* simple error logging for failed device.  */

static void
log_dev_error(const char *host, const char *str)
{
	log_message(LOGT_BG, LOGL_CRIT, "%s for device %s", str, host);
}


#define	MAX_HOST_IDS	64

int
ls_set_searchlist(ls_search_handle_t handle, int num_groups,
		  groupid_t * glist)
{
	search_context_t *sc;
	groupid_t       cur_gid;
	device_handle_t *cur_dev;
	char		*hosts[MAX_HOST_IDS];
	int             nhosts;
	int             i,
	                j;
	int             err;
	char 			buf[MAX_LOG_ENTRY], gbuf[MAX_GID_NAME];
	
	sc = (search_context_t *) handle;

	buf[0]='\0';
	for (i = 0; i < num_groups; i++) {
		sprintf(gbuf, "%d ", (int) glist[i]);
		strcat(buf, gbuf);
	}

	log_message(LOGT_BG, LOGL_TRACE, "ls_set_searchlist: groups(%d) %s",
		    num_groups, buf);
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
		err = device_clear_gids(cur_dev->dev_handle);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name, "failed clear gid");
		}
	}


	/*
	 * for each of the groups, get the list
	 * of machines that have some of this data.
	 */
	for (i = 0; i < num_groups; i++) {
		cur_gid = glist[i];
		nhosts = MAX_HOST_IDS;
		glkup_gid_hosts(cur_gid, &nhosts, hosts);
		for (j = 0; j < nhosts; j++) {
			err = device_add_gid(sc, cur_gid, hosts[j]);
			if (err) {
				log_dev_error(hosts[j], "Failed to add gid");
			}
		}
	}
	return (0);
}

static int
cache_file(char *oname, sig_val_t *sig)
{
	size_t	olen; 
	char *	buf;
	ssize_t rsize;
	struct stat stats;
	char * cdir;
	int err;
	int fd;
	FILE *cur_file;
	char *sstr;
	char name_buf[PATH_MAX];

	/* read the object file into memory */

	err = stat(oname, &stats);
	if (err) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed stat object file <%s>", oname);
		return (ENOENT);
	}
	olen = stats.st_size;
	buf = malloc(olen);
	if ((cur_file = fopen(oname, "r")) == NULL) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed open object <%s>", oname);
		free(buf);
       		return (ENOENT);
       	}
	if ((rsize = fread(buf, olen, 1, cur_file)) != 1) { 
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed read object <%s>", oname); 
		free(buf); 
		return (EAGAIN);
	}
	fclose(cur_file);

		
	

	/* compute the signature of the file */
	sig_cal(buf, olen, sig);

	cdir = dconf_get_binary_cachedir();
	sstr = sig_string(sig);
	snprintf(name_buf, PATH_MAX, SO_FORMAT, cdir, sstr);

	/* create the new file */
	file_get_lock(name_buf);
	fd = open(name_buf, O_CREAT|O_EXCL|O_WRONLY, 0744);
	if (fd < 0) {
		file_release_lock(name_buf);
		if (errno == EEXIST) {
		  err = 0;
		  goto done;
		}
		fprintf(stderr, "file %s failed on %d \n",
			name_buf, errno);
		err = errno;
		goto done;

	}
	if (write(fd, buf, olen) != olen) {
		perror("write buffer file");

	}
	// XXX
	err = 0;
	close(fd);
	file_release_lock(name_buf);
done:
	free(buf);
	free(cdir);	
	free(sstr);	
	return(err);
}

static int
cache_spec(char *oname, sig_val_t *sig)
{
	size_t	olen; 
	char *	buf;
	ssize_t rsize;
	struct stat stats;
	char * cdir;
	int err;
	int fd;
	FILE *cur_file;
	char *sstr;
	char name_buf[PATH_MAX];

	/* read the object file into memory */

	err = stat(oname, &stats);
	if (err) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed stat object file <%s>", oname);
		return (ENOENT);
	}
	olen = stats.st_size;
	buf = malloc(olen);
	if ((cur_file = fopen(oname, "r")) == NULL) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed open object <%s>", oname);
		free(buf);
       		return (ENOENT);
       	}
	if ((rsize = fread(buf, olen, 1, cur_file)) != 1) { 
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed read object <%s>", oname); 
		free(buf); 
		return (EAGAIN);
	}
	fclose(cur_file);


	/* compute the signature of the file */
	sig_cal(buf, olen, sig);

	cdir = dconf_get_spec_cachedir();
	sstr = sig_string(sig);
	snprintf(name_buf, PATH_MAX, SPEC_FORMAT, cdir, sstr);

	file_get_lock(name_buf);
	/* create the new file */
	fd = open(name_buf, O_CREAT|O_EXCL|O_WRONLY, 0744);
	if (fd < 0) {
		file_release_lock(name_buf);
		if (errno == EEXIST) {
		  err = 0;
		  goto done;
		}
		fprintf(stderr, "file %s failed on %d \n",
			name_buf, errno);
		err = errno;
		goto done;

	}
	if (write(fd, buf, olen) != olen) {
		perror("write buffer file");

	}
	// XXX
	err = 0;

	close(fd);
	file_release_lock(name_buf);
done:
	free(buf);
	free(cdir);	
	free(sstr);	
	return(err);
}

int
ls_set_searchlet(ls_search_handle_t handle, device_isa_t isa_type,
		 char *filter_file_name, char *filter_spec_name)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;
	int             started = 0;
	sig_val_t	obj_sig;
	sig_val_t	spec_sig;

	sc = (search_context_t *) handle;

	/*
	 * XXX do something with the isa_type !! 
	 */

	/*
	 * if state is active, we can't change the searchlet.
	 * XXX what other states are not valid ??
	 */
	if (sc->cur_status == SS_ACTIVE) {
		/*
		 * XXX log 
		 */
		printf("still idle \n");
		return (EINVAL);
	}

	/* change the search id */
	sc->Xcur_search_id++;

	/* copy object file to the object cache directory */
	if (cache_spec(filter_spec_name, &spec_sig)) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed to cache spec <%s>", 
		    filter_spec_name); 
		return(EINVAL);
	}


	/* copy object file to the object cache directory */
	if (cache_file(filter_file_name, &obj_sig)) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed to cache objectt <%s>", 
		    filter_file_name); 
		return(EINVAL);
	}

	log_message(LOGT_BG, LOGL_TRACE, "ls_set_searchlet: filter %s spec %s",
		    filter_file_name, filter_spec_name);

	/*
	 * we need to verify the searchlet somehow 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_spec(cur_dev->dev_handle, filter_spec_name,
				      &spec_sig);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name, "failed setting spec");
		}
		err = device_set_lib(cur_dev->dev_handle, &obj_sig);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name,
			    "failed setting searchlet");
		} else {
			started++;
		}
	}

	err = bg_set_spec(sc, &spec_sig);
	if (err) {
		/*
		 * XXX log 
		 */
		assert(0);
	}

	err = bg_set_lib(sc, &obj_sig);
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
	sig_val_t	obj_sig;

	sc = (search_context_t *) handle;

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

	/* copy object file to the object cache directory */
	if (cache_file(filter_file_name, &obj_sig)) {
		log_message(LOGT_BG, LOGL_ERR, 
		    "ls_set_searchlet: failed to cache objectt <%s>", filter_file_name); 
		return(EINVAL);
	}

	log_message(LOGT_BG, LOGL_TRACE, "ls_add_filter_file: filter %s",
		    filter_file_name);
	/*
	 * we need to verify the searchlet somehow 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_lib(cur_dev->dev_handle, &obj_sig);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name,
			    "failed adding filter file");
		} else {
			started++;
		}
	}

	/* XXX */	
	err = bg_set_lib(sc, &obj_sig);
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

	if (sc->cur_status == SS_ACTIVE) {
		/*
		 * XXX log 
		 */
		fprintf(stderr, " Search is active \n");
		return (EBUSY);
	}

	log_message(LOGT_BG, LOGL_TRACE,
		    "ls_set_blob: filter %s data %x len %d",
		    filter_name, blob_data, blob_len);
	/*
	 * we need to verify the searchlet somehow 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_blob(cur_dev->dev_handle, filter_name,
				      blob_len, blob_data);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name, "failed to set blob");
		}
	}

	err = bg_set_blob(sc, filter_name, blob_len, blob_data);
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

	memset(&sc->host_stats, 0, sizeof(host_stats_t));
	
	err = bg_start_search(sc);
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
		err = device_start(cur_dev->dev_handle);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name,
			    "failed starting search");
		} else {
			started++;
		}
	}

	/*
	 * XXX 
	 */
	started++;

	log_message(LOGT_BG, LOGL_TRACE, "ls_start_search");

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

int
ls_abort_search(ls_search_handle_t handle)
{
  app_stats_t astats;

  memset(&astats, 0, sizeof(app_stats_t));
  return (ls_abort_search_extended(handle, &astats));
}

int
ls_abort_search_extended(ls_search_handle_t handle, app_stats_t *as)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err;
	int             ret_err;

	sc = (search_context_t *) handle;

	/*
	 * If no search is currently active (or just completed) then
	 * this is an error.
	 */
	if ((sc->cur_status != SS_ACTIVE) && (sc->cur_status != SS_DONE)) {
		return (EINVAL);
	}

	log_message(LOGT_BG, LOGL_TRACE, "ls_abort_search");
		
	err = bg_stop_search(sc);
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
		sc->host_stats.hs_objs_uqueued = as->as_objs_queued;
		sc->host_stats.hs_objs_upresented = as->as_objs_presented;
		err = device_stop(cur_dev->dev_handle, &sc->host_stats);
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
	sc->Xcur_search_id++;

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
	struct timespec timeout;
	char	       *sigstr;

	/*
	 * XXX make sure search is running 
	 */
	sc = (search_context_t *) handle;

	log_message(LOGT_BG, LOGL_TRACE, "ls_next_object");

	/*
	 * Try to get an item from the queue, if data is not available,
	 * then we either spin or return EWOULDBLOCK based on the
	 * flags.
	 */
      again:
	while ((obj_data = ring_deq(sc->proc_ring)) == NULL) {

		/*
		 * Make sure we are still processing data.
		 */

		if (sc->cur_status == SS_DONE) {
			log_message(LOGT_BG, LOGL_TRACE, 
				    "ls_next_object: --> no more objects");
			return (ENOENT);
		}


		/*
		 * See if we are blocking or non-blocking.
		 */
		if ((flags & LSEARCH_NO_BLOCK) == LSEARCH_NO_BLOCK) {
			log_message(LOGT_BG, LOGL_TRACE, 
				    "ls_next_object: --> would block");
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
#warning "Check this"
#if 0
	if (sc->cur_search_id != obj_info->ver_num) {
		ls_release_object(sc, obj_data);
		goto again;
	}
#endif
	sc->host_stats.hs_objs_read++;
	/*
	 * XXX how should we get this state really ?? 
	 */
	obj_data->cur_offset = 0;
	obj_data->cur_blocksize = 1024;

	*obj_handle = (ls_obj_handle_t *) obj_data;
	sigstr = sig_string(&obj_data->id_sig);
	if(sigstr != NULL) {
	  log_message(LOGT_BG, LOGL_TRACE, "ls_next_object: --> %s", sigstr);
	  free(sigstr);
	}

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

	dev = (device_handle_t *) dev_handle;

	/*
	 * check that this is a valid argument 
	 */
	if (dev == NULL) {
		return (EINVAL);
	}

	return device_statistics(dev->dev_handle, dev_stats, stat_len);
}

int
ls_get_dev_session_variables(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
			     device_session_vars_t **vars)
{
  device_handle_t *dev;
  search_context_t *sc;

  sc = (search_context_t *) handle;

  dev = (device_handle_t *) dev_handle;

  /*
   * check that this is a valid argument
   */
  if (dev == NULL) {
    return (EINVAL);
  }

  return device_get_session_variables(dev->dev_handle, vars);
}

int
ls_set_dev_session_variables(ls_search_handle_t handle, ls_dev_handle_t dev_handle,
			     device_session_vars_t *vars)
{
  device_handle_t *dev;
  search_context_t *sc;

  sc = (search_context_t *) handle;

  dev = (device_handle_t *) dev_handle;

  /*
   * check that this is a valid argument
   */
  if (dev == NULL) {
    return (EINVAL);
  }

  return device_set_session_variables(dev->dev_handle, vars);
}


/*!
 * This call advises Diamond of the user's state.  
 *
 * \param handle
 *		the search handle returned by init_libsearchlet().
 *
 * \return 0
 *		The state was set successfully.
 *
 * \return EINVAL
 *		There was no active search or the handle is invalid.
 */

int ls_set_user_state(ls_search_handle_t handle, user_state_t state)
{
	search_context_t *sc;
	device_handle_t *cur_dev;
	int             err = 0;
	filter_exec_mode_t new_mode;

	sc = (search_context_t *) handle;

	log_message(LOGT_BG, LOGL_TRACE, "ls_set_user_state: state %s",
		    state == USER_BUSY ? "BUSY" : "WAITING");

	/*
	 * set execution mode to current for waiting users,
	 * and hybrid for busy users.
	 */
	if (state == USER_WAITING) {
	 	new_mode = FM_CURRENT;
	} else {
	 	new_mode = FM_HYBRID;
	}

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		err = device_set_user_state(cur_dev->dev_handle, state);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name,
				      "failed to set user state");
			return(err);
		}
		
		err = device_set_exec_mode(cur_dev->dev_handle, new_mode);
		if (err != 0) {
			log_dev_error(cur_dev->dev_name,
				      "failed to set exec mode");
			return(err);
		}
				
	}
	
	return(err);
}

