/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 2
 *
 *  Copyright (c) 2002-2005 Intel Corporation
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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include "ring.h"
#include "rstat.h"
#include "lib_searchlet.h"
#include "obj_attr.h"
#include "lib_od.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "lib_hstub.h"
#include "lib_log.h"
#include "filter_exec.h"
#include "filter_priv.h"        /* to read stats -RW */
#include "idiskd_ops.h"
#include "dctl_common.h"
#include "id_search_priv.h"
#include "lib_dconfig.h"


static char const cvsid[] = "$Header$";

/*
 * XXX other place 
 */
extern int      do_cleanup;

/*
 * XXX move to seperate header file !!! 
 */
#define	CONTROL_RING_SIZE	512

static int      search_free_obj(search_state_t *sstate, obj_data_t * obj);

typedef enum {
    DEV_STOP,
    DEV_TERM,
    DEV_START,
    DEV_SEARCHLET,
    DEV_BLOB,
    DEV_GID
} dev_op_type_t;


typedef struct
{
	char           *filter;
	char           *spec;
}
dev_slet_data_t;

typedef struct
{
	char           *fname;
	void           *blob;
	int             blen;
}
dev_blob_data_t;

typedef struct
{
	dev_op_type_t   cmd;
	int             id;
	union {
		dev_slet_data_t sdata;
		dev_blob_data_t bdata;
		groupid_t		gid;
	} extra_data;
}
dev_cmd_data_t;

extern char    *data_dir;

/*
 * XXX clean this up later 
 */

int             cpu_split = 0;
int             cpu_split_thresh = RAND_MAX;


int
ls_release_object(ls_search_handle_t handle, ls_obj_handle_t obj_handle)
{
	obj_data_t *    new_obj;
	obj_adata_t     *cur, *next;

	new_obj = (obj_data_t *)obj_handle;

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
	return(0);
}

static int
release_object(ls_obj_handle_t obj_handle)
{
	obj_data_t *    new_obj;
	obj_adata_t     *cur, *next;

	new_obj = (obj_data_t *)obj_handle;

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
}

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
thread_setup(search_state_t * sstate)
{
	log_thread_register(sstate->log_cookie);
	dctl_thread_register(sstate->dctl_cookie);
}

int
search_stop(void *app_cookie, int gen_num)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;
	int             err;

	sstate = (search_state_t *) app_cookie;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_STOP;
	cmd->id = gen_num;

	err = ring_enq(sstate->control_ops, (void *) cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}


int
search_term(void *app_cookie, int id)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;
	int             err;

	sstate = (search_state_t *) app_cookie;

	/*
	 * Allocate a new command and put it onto the ring
	 * of commands being processed.
	 */
	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}
	cmd->cmd = DEV_TERM;
	cmd->id = id;

	/*
	 * Put it on the ring.
	 */
	err = ring_enq(sstate->control_ops, (void *) cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}

int
search_setlog(void *app_cookie, uint32_t level, uint32_t src)
{
	uint32_t        hlevel,
	hsrc;

	hlevel = ntohl(level);
	hsrc = ntohl(src);

	log_setlevel(hlevel);
	log_settype(hsrc);
}



int
search_start(void *app_cookie, int id)
{
	dev_cmd_data_t *cmd;
	int             err;
	search_state_t *sstate;

	/*
	 * XXX start 
	 */

	sstate = (search_state_t *) app_cookie;
	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_START;
	cmd->id = id;


	err = ring_enq(sstate->control_ops, (void *) cmd);
	if (err) {
		free(cmd);
		return (1);
	}

	return (0);
}


/*
 * This is called to set the searchlet for the current search.
 */

int
search_set_searchlet(void *app_cookie, int id, char *filter, char *spec)
{
	dev_cmd_data_t *cmd;
	int             err;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_SEARCHLET;
	cmd->id = id;

	cmd->extra_data.sdata.filter = filter;
	cmd->extra_data.sdata.spec = spec;

	err = ring_enq(sstate->control_ops, (void *) cmd);
	if (err) {
		free(cmd);
		return (1);
	}
	return (0);
}


/*
 * Reset the statistics for the current search.
 */
void
clear_ss_stats(search_state_t * sstate)
{
	sstate->obj_total = 0;
	sstate->obj_processed = 0;
	sstate->obj_dropped = 0;
	sstate->obj_passed = 0;
	sstate->obj_skipped = 0;
}


/*
 * This initites a search on the disk.  For this to work, 
 * we need to make sure that we have searchlets set for all
 * the devices and that we have an appropraite search_set.
 */
static int
id_start_search(search_state_t *sstate)
{
	device_handle_t		*cur_dev;
	int			err;
	int			started = 0;
	time_t			cur_time;

	thread_setup(sstate);

	/*
	 * if state isn't idle, then we haven't set the searchlet on
	 * all the devices, so this is an error condition.
	 *
	 * If the cur_status == ACTIVE, it is already running which
	 * is also an error.
	 *
	 */
	if (sstate->cur_status != SS_IDLE) {
		/* XXX log */
		return (EINVAL);
	}

	err = bg_start_search(sstate, sstate->cur_search_id);
	if (err) {
		/* XXX log */
	}

	time(&cur_time);

	cur_dev = sstate->dev_list;
	while (cur_dev != NULL) {
		/* clear the complete flag */
		cur_dev->flags &= ~DEV_FLAG_COMPLETE;
		cur_dev->start_time = cur_time;
		err = device_start(cur_dev->dev_handle, sstate->cur_search_id);
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

	printf("end start!!\n");
	/* XXX */
	started++;

	if (started > 0) {
		sstate->flags |= DEV_FLAG_RUNNING;
		sstate->cur_status = SS_ACTIVE;
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
id_stop_search(search_state_t *sstate)
{
	device_handle_t		*cur_dev;
	int			err;
	int			ret_err;

	thread_setup(sstate);

	/*
	 * If no search is currently active (or just completed) then
	 * this is an error.
	 */
	if ((sstate->cur_status != SS_ACTIVE) && (sstate->cur_status != SS_DONE)) {
		return (EINVAL);
	}

	err = bg_stop_search(sstate, sstate->cur_search_id);
	if (err) {
		/* XXX log */
	}

	cur_dev = sstate->dev_list;

	ret_err = 0;

	while (cur_dev != NULL) {
		err = device_stop(cur_dev->dev_handle, sstate->cur_search_id);
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
	sstate->cur_search_id++;

	/* change the current state to idle */
	sstate->cur_status = SS_IDLE;
	sstate->flags &= ~DEV_FLAG_RUNNING;
	return (ret_err);

}


int
id_set_searchlet(search_state_t *sstate, char *filter_file_name,
                 char *filter_spec_name, int vid)
{
	device_handle_t		*cur_dev;
	int			err;
	int			started = 0;

	thread_setup(sstate);

	/*
	 * if state is active, we can't change the searchlet.
	 * XXX what other states are no valid ??
	 */
	if (sstate->cur_status == SS_ACTIVE) {
		/* XXX log */
		printf("still active \n");
		return (EINVAL);
	}
	/* change the search id */
	sstate->cur_search_id = vid;

	/* we need to verify the searchlet somehow */
	cur_dev = sstate->dev_list;
	while (cur_dev != NULL) {
		err = device_set_searchlet(cur_dev->dev_handle,
		                           sstate->cur_search_id, filter_file_name, filter_spec_name);
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

	err = bg_set_searchlet(sstate, sstate->cur_search_id,
	                       filter_file_name, filter_spec_name);
	if (err) {
		/* XXX log */
	}

	/* XXXX */
	sstate->cur_status = SS_IDLE;

	return(0);
}


int
id_set_blob(search_state_t *sstate, char *filter_name,
            int  blob_len, void *blob_data)
{

	device_handle_t		*cur_dev;
	int					err;

	thread_setup(sstate);

	if (sstate->cur_status == SS_ACTIVE) {
		/* XXX log */
		fprintf(stderr, " Search is active \n");
		return (EBUSY);
	}

	/* we need to verify the searchlet somehow */
	for (cur_dev = sstate->dev_list; cur_dev != NULL; cur_dev= cur_dev->next) {
		err = device_set_blob(cur_dev->dev_handle,
		                      sstate->cur_search_id, filter_name, blob_len, blob_data);
		if (err != 0) {
			/*
			 * It isn't obvious what we need to do if we
			 * get an error here.  This applies to the fault
			 * tolerance story.  For now we keep trying the
			 * rest of the device
			 * XXX figure out what to do here ???
			 */
			/* XXX logging */
			assert(0);
		}
	}

	err = bg_set_blob(sstate, sstate->cur_search_id, filter_name, blob_len,
	                  blob_data);
	if (err) {
		/* XXX log */
	}

	return(0);
}

#define	MAX_HOST_IDS	40	/* XXX */
void
id_set_gid(search_state_t *sstate, groupid_t gid)
{
	uint32_t        host_ids[MAX_HOST_IDS];
	int         hosts;
	int         j;
	int         err;


	/*
	 * for each of the groups, get the list
	 * of machines that have some of this data.
	 */
	hosts = MAX_HOST_IDS;
	glkup_gid_hosts(gid, &hosts, host_ids);
	printf("gid match %d hosts \n", hosts);
	for (j=0; j<hosts; j++) {
		err = device_add_gid(sstate, gid, host_ids[j]);
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
			fprintf(stderr, "Failed to connect to device %s for gid %llx\n",                        name, gid);
		}
	}

	/* XXX push the list of groups to disk */



}

/*
 * Take the current command and process it.  Note, the command
 * will be free'd by the caller.
 */
static void
dev_process_cmd(search_state_t * sstate, dev_cmd_data_t * cmd)
{
	int             err;
	char           *obj_name;
	char           *spec_name;



	switch (cmd->cmd) {
		case DEV_STOP:
			/*
			 * Stop the current search by 
			 *
			 */
			id_stop_search(sstate);

			/*
			 * flush objects in the transmit queue 
			 */
			/* XXX do we need to do this */
			err = sstub_flush_objs(sstate->comm_cookie, sstate->ver_no);
			assert(err == 0);

			break;

		case DEV_TERM:
			break;

		case DEV_START:

			/*
			 * Clear the stats.
			 */
			printf("starting \n");
			clear_ss_stats(sstate);
			id_start_search(sstate);
			break;

		case DEV_SEARCHLET:
			printf("setting searchlet \n");
			sstate->ver_no = cmd->id;

			obj_name = cmd->extra_data.sdata.filter;
			spec_name = cmd->extra_data.sdata.spec;

			id_set_searchlet(sstate, obj_name, spec_name, cmd->id);
			// XXX free(obj_name);
			// XXX free(spec_name);

			break;

		case DEV_BLOB: {

				char           *name;
				int             blen;
				void           *blob;

				name = cmd->extra_data.bdata.fname;
				blen = cmd->extra_data.bdata.blen;
				blob = cmd->extra_data.bdata.blob;
				id_set_blob(sstate, name, blen, blob);
				// free(name);
				// free(blob);
				break;
			}

		case DEV_GID:
			printf("setting GID \n");
			id_set_gid(sstate, cmd->extra_data.gid);
			break;

		default:
			printf("unknown command %d \n", cmd->cmd);
			break;

	}
}




static int
continue_fn(void *cookie)
{
	return(1);
}

/*
 * This is the main thread that executes a "search" on a device.
 * This interates it handles incoming messages as well as processing
 * object.
 */

static void    *
device_main(void *arg)
{
	search_state_t *sstate;
	dev_cmd_data_t *cmd;
	obj_data_t     *new_obj;
	obj_info_t *	obj_info;
	int             err;
	int             any;
	int				complete;
	double			time;
	struct timespec timeout;
	int		force_eval;


	sstate = (search_state_t *) arg;

	log_thread_register(sstate->log_cookie);
	dctl_thread_register(sstate->dctl_cookie);

	/*
	 * XXX need to open comm channel with device
	 */


	while (1) {
		any = 0;
		cmd = (dev_cmd_data_t *) ring_deq(sstate->control_ops);
		if (cmd != NULL) {
			any = 1;
			dev_process_cmd(sstate, cmd);
			free(cmd);
		}

		/*
		 * XXX look for data from device to process.
		 */
		if ((sstate->flags & DEV_FLAG_RUNNING) &&
		    (sstub_queued_objects(sstate->comm_cookie) < 40)) {
			obj_info = (obj_info_t *)ring_deq(sstate->proc_ring);
			if (obj_info != NULL) {
				any = 1;
				new_obj = obj_info->obj;
				err = sstub_send_obj(sstate->comm_cookie,
				                     new_obj, sstate->ver_no, 1);
				assert(err == 0);
				free(obj_info);
			} else if (sstate->cur_status == SS_DONE) {
				sstate->flags &= ~DEV_FLAG_RUNNING;
				sstate->flags |= DEV_FLAG_COMPLETE;
				new_obj = odisk_null_obj();
				new_obj->remain_compute = 0.0;
				err = sstub_send_obj(sstate->comm_cookie,
				                     new_obj, sstate->ver_no, 1);
			}
		}
#ifdef	XXX
		ls_release_object(sc, new_obj);

		cmd = (dev_cmd_data_t *) ring_deq(sstate->control_ops);
		force_eval = 0;
		/* XXXX change this */
		if (err == ENOENT) {
			/*
			 * We have processed all the objects,
			 * clear the running and set the complete
			 * flags.
			 */
			sstate->flags &= ~DEV_FLAG_RUNNING;
			sstate->flags |= DEV_FLAG_COMPLETE;

			/*
			 * XXX big hack for now.  To indiate
			 * we are done we send object with
			 * no data or attributes.
			 */
			new_obj = odisk_null_obj();
			new_obj->remain_compute = 0.0;
			err = sstub_send_obj(sstate->comm_cookie,
			                     new_obj, sstate->ver_no, 1);
			if (err) {
				/*
				 * XXX overflow gracefully 
				 */
				/*
				 * XXX log 
				 */

			} else {
				/*
				 * XXX log 
				 */
				sstate->pend_objs++;
			}
		} else if (err) {
			/*
			 * printf("dmain: failed to get obj !! \n"); 
			 */
			/*
			 * sleep(1); 
			 */
			continue;
		} else {
			any = 1;

			/*
				 * set bypass values periodically.
				 */
			sstate->obj_processed++;


			/*
			 * We want to process some number of objects
			 * locally to get the necessary statistics.
			 * Setting force_eval will make sure process the whole
			 * object.
			 */

			if ((sstate->obj_processed & 0xf) == 0xf) {
				force_eval = 1;
			}

			err = eval_filters(new_obj, sstate->fdata, force_eval, &time,
			                   sstate, continue_fn, NULL);
			if (err == 0) {
				sstate->obj_dropped++;
				search_free_obj(sstate, new_obj);
			} else {
				sstate->obj_passed++;
				if (err == 1) {
					complete = 0;
				} else {
					complete = 1;
				}

				sstate->pend_objs++;
				sstate->pend_compute += new_obj->remain_compute;
				//printf("queu %f new %f \n",
				//new_obj->remain_compute,
				//sstate->pend_compute);

				err = sstub_send_obj(sstate->comm_cookie, new_obj,
				                     sstate->ver_no, complete);
				if (err) {
					/*
					 * XXX overflow gracefully
					 */
					sstate->pend_objs--;
					sstate->pend_compute -= new_obj->remain_compute;
				}
			}
		}
	}
#endif

	/*
	 * If we didn't have any work to process this time around,
	 * then we sleep on a cond variable for a small amount
	 * of time.
	 */
	/*
	 * XXX move mutex's to the state data structure 
	 */
	if (!any) {
		timeout.tv_sec = 0;
		timeout.tv_nsec = 100000000; /* XXX 10ms */
		nanosleep(&timeout, NULL);
	}
}
}



/*
 * This is called when we have finished sending a log entry.  For the 
 * time being, we ignore the arguments because we only have one
 * request outstanding.  This sets a condition value to wake up
 * the main thread.
 */
int
search_log_done(void *app_cookie, char *buf, int len)
{
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	pthread_mutex_lock(&sstate->log_mutex);
	pthread_cond_signal(&sstate->log_cond);
	pthread_mutex_unlock(&sstate->log_mutex);

	return (0);
}


static void    *
log_main(void *arg)
{
	search_state_t *sstate;
	char           *log_buf;
	int             err;
	struct timeval  now;
	struct timespec timeout;
	struct timezone tz;
	int             len;

	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;

	sstate = (search_state_t *) arg;
	log_thread_register(sstate->log_cookie);
	dctl_thread_register(sstate->dctl_cookie);

	while (1) {

		len = log_getbuf(&log_buf);
		if (len > 0) {

			/*
			 * send the buffer 
			 */
			err = sstub_send_log(sstate->comm_cookie, log_buf, len);
			if (err) {
				/*
				 * probably shouldn't happen
				 * but we ignore and return the data
				 */
				log_advbuf(len);
				continue;
			}

			/*
			 * wait on cv for the send to complete 
			 */
			pthread_mutex_lock(&sstate->log_mutex);
			pthread_cond_wait(&sstate->log_cond, &sstate->log_mutex);
			pthread_mutex_unlock(&sstate->log_mutex);

			/*
			 * let the log library know this space can
			 * be re-used.
			 */
			log_advbuf(len);

		} else {
			gettimeofday(&now, &tz);
			pthread_mutex_lock(&sstate->log_mutex);
			timeout.tv_sec = now.tv_sec + 1;
			timeout.tv_nsec = now.tv_usec * 1000;

			pthread_cond_timedwait(&sstate->log_cond,
			                       &sstate->log_mutex, &timeout);
			pthread_mutex_unlock(&sstate->log_mutex);
		}
	}
}





/*
 * This is the callback that is called when a new connection
 * has been established at the network layer.  This creates
 * new search contect and creates a thread to process
 * the data. 
 */
int
search_new_conn(void *comm_cookie, void **app_cookie)
{
	search_state_t *sstate;
	int             err;

	sstate = (search_state_t *) malloc(sizeof(*sstate));
	if (sstate == NULL) {
		*app_cookie = NULL;
		return (ENOMEM);
	}

	memset((void *) sstate, 0, sizeof(*sstate));
	/*
	 * Set the return values to this "handle".
	 */
	*app_cookie = sstate;
	sstate->cur_status = SS_IDLE;
	sstate->pend_lw = 55; /* XXX magic const */

	/*
	 * This is called in the new process, now we initializes it
	 * log data.
	 */

	log_init(&sstate->log_cookie);
	dctl_init(&sstate->dctl_cookie);

	dctl_register_node(ROOT_PATH, SEARCH_NAME);

	dctl_register_leaf(DEV_SEARCH_PATH, "version_num",
	                   DCTL_DT_UINT32, dctl_read_uint32, NULL,
	                   &sstate->ver_no);
	dctl_register_leaf(DEV_SEARCH_PATH, "obj_total", DCTL_DT_UINT32,
	                   dctl_read_uint32, NULL, &sstate->obj_total);
	dctl_register_leaf(DEV_SEARCH_PATH, "obj_processed", DCTL_DT_UINT32,
	                   dctl_read_uint32, NULL, &sstate->obj_processed);
	dctl_register_leaf(DEV_SEARCH_PATH, "obj_dropped", DCTL_DT_UINT32,
	                   dctl_read_uint32, NULL, &sstate->obj_dropped);
	dctl_register_leaf(DEV_SEARCH_PATH, "obj_pass", DCTL_DT_UINT32,
	                   dctl_read_uint32, NULL, &sstate->obj_passed);
	dctl_register_leaf(DEV_SEARCH_PATH, "obj_skipped", DCTL_DT_UINT32,
	                   dctl_read_uint32, NULL, &sstate->obj_skipped);
	dctl_register_leaf(DEV_SEARCH_PATH, "pend_objs", DCTL_DT_UINT32,
	                   dctl_read_uint32, NULL, &sstate->pend_objs);
	dctl_register_leaf(DEV_SEARCH_PATH, "pend_thresh", DCTL_DT_UINT32,
	                   dctl_read_uint32, dctl_write_uint32,
	                   &sstate->pend_thresh);
	dctl_register_leaf(DEV_SEARCH_PATH, "bp_feedback", DCTL_DT_UINT32,
	                   dctl_read_uint32, dctl_write_uint32,
	                   &sstate->bp_feedback);
	dctl_register_leaf(DEV_SEARCH_PATH, "bp_thresh", DCTL_DT_UINT32,
	                   dctl_read_uint32, dctl_write_uint32,
	                   &sstate->bp_feedback);

	err = dctl_register_node(ROOT_PATH, HOST_PATH);
	assert(err == 0);

	err = dctl_register_node(ROOT_PATH, DEVICE_PATH);
	assert(err == 0);

	err = dctl_register_node(HOST_PATH, HOST_NETWORK_NODE);
	assert(err == 0);


	dctl_register_node(ROOT_PATH, DEV_NETWORK_NODE);

	dctl_register_node(ROOT_PATH, DEV_FEXEC_NODE);


	log_start(sstate);
	//dctl_start(sstate);

	bg_init(sstate, 1);

	/*
	 * initialize libfilterexec
	 */
	fexec_system_init();

	/*
	 * init the ring to hold the queue of pending operations.
	 */
	err = ring_init(&sstate->control_ops, CONTROL_RING_SIZE);
	if (err) {
		free(sstate);
		*app_cookie = NULL;
		return (ENOENT);
	}

	/* XXX fix this */
	err = ring_init(&sstate->proc_ring, 512 );
	if (err) {
		free(sstate);
		*app_cookie = NULL;
		return (ENOENT);
	}



	sstate->flags = 0;
	sstate->comm_cookie = comm_cookie;

	sstate->pend_thresh = SSTATE_DEFAULT_OBJ_THRESH;
	sstate->pend_objs = 0;

	sstate->bp_feedback = 0;
	sstate->bp_thresh = SSTATE_DEFAULT_BP_THRESH;
	/*
	 * Create a new thread that handles the searches for this current
	 * search.  (We probably want to make this a seperate process ??).
	 */

	err = pthread_create(&sstate->thread_id, PATTR_DEFAULT, device_main,
	                     (void *) sstate);
	if (err) {
		/*
		 * XXX log 
		 */
		free(sstate);
		*app_cookie = NULL;
		return (ENOENT);
	}

	/*
	 * Now we also setup a thread that handles getting the log
	 * data and pusshing it to the host.
	 */
	pthread_cond_init(&sstate->log_cond, NULL);
	pthread_mutex_init(&sstate->log_mutex, NULL);
	err = pthread_create(&sstate->log_thread, PATTR_DEFAULT, log_main,
	                     (void *) sstate);
	if (err) {
		/*
		 * XXX log 
		 */
		free(sstate);
		return (ENOENT);
	}
	return (0);
}





/*
 * a request to get the characteristics of the device.
 */
int
search_get_char(void *app_cookie, int gen_num)
{
	device_char_t   dev_char;
	search_state_t *sstate;
	u_int64_t       val;
	int             err;


	sstate = (search_state_t *) app_cookie;

	/* XXX FIX */
	dev_char.dc_isa = DEV_ISA_IA32;
	dev_char.dc_speed = (r_cpu_freq(&val) ? 0 : val);
	dev_char.dc_mem = (r_freemem(&val) ? 0 : val);

	/*
	 * XXX 
	 */
	err = sstub_send_dev_char(sstate->comm_cookie, &dev_char);

	return 0;
}



int
search_close_conn(void *app_cookie)
{
	return (0);
}

int
search_free_obj(search_state_t *sstate, obj_data_t * obj)
{
	release_object(obj);
	return(0);
}

/*
 * This releases an object that is no longer needed.
 */

int
search_release_obj(void *app_cookie, obj_data_t * obj)
{
	search_state_t *sstate;
	sstate = (search_state_t *) app_cookie;

	if (obj == NULL) {
		return(0);
	}

	sstate->pend_objs--;
	sstate->pend_compute -= obj->remain_compute;

	release_object(obj);
	return (0);

}


int
search_set_list(void *app_cookie, int gen_num)
{
	/*
	 * printf("XXX set list \n"); 
	 */
	return (0);
}

/*
 * Get the current statistics on the system.
 */

void
search_get_stats(void *app_cookie, int gen_num)
{
	search_state_t *sstate;
	dev_stats_t    *stats;
	int             err;
	int             num_filt;
	int             len;

	sstate = (search_state_t *) app_cookie;

	/*
	 * Figure out how many filters we have an allocate
	 * the needed space.
	 */
	num_filt = fexec_num_filters(sstate->fdata);
	len = DEV_STATS_SIZE(num_filt);

	stats = (dev_stats_t *) malloc(len);
	if (stats == NULL) {
		/*
		 * This is a periodic poll, so we can ingore this
		 * one if we don't have enough state.
		 */
		log_message(LOGT_DISK, LOGL_ERR, "search_get_stats: no mem");
		return;
	}

	/*
	 * Fill in the state we can handle here.
	 */
	stats->ds_objs_total = sstate->obj_total;
	stats->ds_objs_processed = sstate->obj_processed;
	stats->ds_objs_dropped = sstate->obj_dropped;
	stats->ds_objs_nproc = sstate->obj_skipped;
	stats->ds_system_load = (int) (fexec_get_load(sstate->fdata) * 100.0);  /* XXX
		                                                                             */
	stats->ds_avg_obj_time = 0;
	stats->ds_num_filters = num_filt;


	/*
	 * Get the stats for each filter.
	 */
	err = fexec_get_stats(sstate->fdata, num_filt, stats->ds_filter_stats);
	if (err) {
		free(stats);
		log_message(LOGT_DISK, LOGL_ERR,
		            "search_get_stats: failed to get filter stats");
		return;
	}



	/*
	 * Send the stats.
	 */
	err = sstub_send_stats(sstate->comm_cookie, stats, len);
	free(stats);
	if (err) {
		log_message(LOGT_DISK, LOGL_ERR,
		            "search_get_stats: failed to send stats");
		return;
	}

	return;
}

#define MAX_DBUF    1024
#define MAX_ENTS    512

int
search_read_leaf(void *app_cookie, char *path, int32_t opid)
{

	/*
	 * XXX hack for now 
	 */
	int             len;
	char            data_buf[MAX_DBUF];
	dctl_data_type_t dtype;
	int             err,
	eno;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	len = MAX_DBUF;
	err = dctl_read_leaf(path, &dtype, &len, data_buf);
	/*
	 * XXX deal with ENOSPC 
	 */

	if (err) {
		len = 0;
	}

	eno = sstub_rleaf_response(sstate->comm_cookie, err, dtype, len,
	                           data_buf, opid);
	assert(eno == 0);

	return (0);
}


int
search_write_leaf(void *app_cookie, char *path, int len, char *data,
                  int32_t opid)
{
	/*
	 * XXX hack for now 
	 */
	int             err,
	eno;
	search_state_t *sstate;
	sstate = (search_state_t *) app_cookie;

	err = dctl_write_leaf(path, len, data);
	/*
	 * XXX deal with ENOSPC 
	 */

	if (err) {
		len = 0;
	}

	eno = sstub_wleaf_response(sstate->comm_cookie, err, opid);
	assert(eno == 0);

	return (0);
}

int
search_list_leafs(void *app_cookie, char *path, int32_t opid)
{

	/*
	 * XXX hack for now 
	 */
	int             err,
	eno;
	dctl_entry_t    ent_data[MAX_ENTS];
	int             num_ents;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	num_ents = MAX_ENTS;
	err = dctl_list_leafs(path, &num_ents, ent_data);
	/*
	 * XXX deal with ENOSPC 
	 */

	if (err) {
		num_ents = 0;
	}

	eno = sstub_lleaf_response(sstate->comm_cookie, err, num_ents,
	                           ent_data, opid);
	assert(eno == 0);

	return (0);
}


int
search_list_nodes(void *app_cookie, char *path, int32_t opid)
{

	/*
	 * XXX hack for now 
	 */
	int             err,
	eno;
	dctl_entry_t    ent_data[MAX_ENTS];
	int             num_ents;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	num_ents = MAX_ENTS;
	err = dctl_list_nodes(path, &num_ents, ent_data);
	/*
	 * XXX deal with ENOSPC 
	 */

	if (err) {
		num_ents = 0;
	}

	eno = sstub_lnode_response(sstate->comm_cookie, err, num_ents,
	                           ent_data, opid);
	assert(eno == 0);

	return (0);
}

int
search_set_gid(void *app_cookie, int gen_num, groupid_t gid)
{
	dev_cmd_data_t *cmd;
	int             err;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_GID;
	cmd->id = gen_num;

	cmd->extra_data.gid = gid;

	err = ring_enq(sstate->control_ops, (void *) cmd);
	if (err) {
		free(cmd);
		assert(0);
		return (1);
	}
	return (0);
}


int
search_clear_gids(void *app_cookie, int gen_num)
{
	int             err;
	search_state_t *sstate;

	/*
	 * XXX check gen num 
	 */

	/* XXXX */
	return (0);

}

int
search_set_blob(void *app_cookie, int gen_num, char *name,
                int blob_len, void *blob)
{
	dev_cmd_data_t *cmd;
	int             err;
	search_state_t *sstate;
	void           *new_blob;

	sstate = (search_state_t *) app_cookie;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	new_blob = malloc(blob_len);
	assert(new_blob != NULL);
	memcpy(new_blob, blob, blob_len);

	cmd->cmd = DEV_BLOB;
	cmd->id = gen_num;

	cmd->extra_data.bdata.fname = strdup(name);
	assert(cmd->extra_data.bdata.fname != NULL);
	cmd->extra_data.bdata.blen = blob_len;
	cmd->extra_data.bdata.blob = new_blob;


	err = ring_enq(sstate->control_ops, (void *) cmd);
	if (err) {
		free(cmd);
		assert(0);
		return (1);
	}
	return (0);
}

extern int      fexec_cpu_slowdown;

int
search_set_offload(void *app_cookie, int gen_num, uint64_t load)
{
	double          ratio;
	uint64_t        my_clock;
	double          my_clock_float;
	double          eff_ratio;

	eff_ratio = (double) (double) (100 - fexec_cpu_slowdown) / 100.0;
	/*
	 * XXX clean this up 
	 */
	cpu_split = 1;

	my_clock_float = (double) my_clock *eff_ratio;
	r_cpu_freq(&my_clock);
	ratio = ((double) my_clock) / ((double) load + (double) my_clock);

	cpu_split_thresh = (double) (RAND_MAX) * ratio;

#ifdef	XXX

	printf("set_offload: ratio %f thresh %d cpu %d adj %f \n",
	       ratio, cpu_split_thresh, my_clock, my_clock_float);
#endif

	return (0);
}
