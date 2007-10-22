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
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <values.h>

#include "lib_tools.h"
#include "lib_searchlet.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_hstub.h"
#include "dctl_common.h"
#include "lib_search_priv.h"
#include "lib_filterexec.h"


/*
 * XXX put later 
 */
#define	BG_RING_SIZE	512


#define	BG_STARTED	0x01
#define	BG_SET_SEARCHLET	0x02

/*
 * XXX debug for now, enables cpu based load splitting 
 */
static uint32_t        do_cpu_update = 0;

typedef enum {
	BG_STOP,
	BG_START,
	BG_SPEC,
	BG_LIB,
	BG_SET_BLOB,
} bg_op_type_t;

/*
 * XXX huge hack 
 */
typedef struct {
	bg_op_type_t    cmd;
	bg_op_type_t    ver_id;
	sig_val_t       filt_sig;
	sig_val_t       spec_sig;
	sig_val_t       lib_sig;
	char           *filter_name;
	char           *spec_name;
	void           *blob;
	int             blob_len;
} bg_cmd_data_t;


static void
update_dev_stats(search_context_t * sc)
{
	device_handle_t *cur_dev;
	int             err;
	dev_stats_t    *dstats;
	int             size;
	int             remain;

	dstats = (dev_stats_t *) malloc(DEV_STATS_SIZE(20));
	assert(dstats != NULL);

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		size = DEV_STATS_SIZE(20);
		err = device_statistics(cur_dev->dev_handle, dstats, &size);
		assert(err == 0);

		remain = dstats->ds_objs_total - dstats->ds_objs_processed;
		if (remain < 0)
			remain = 0;
		if (remain == 0) {
			continue;
		}
		cur_dev->obj_total = dstats->ds_objs_total;
		cur_dev->remain_old = cur_dev->remain_mid;
		cur_dev->remain_mid = cur_dev->remain_new;
		cur_dev->remain_new = remain;
		cur_dev->prate = ((float) dstats->ds_avg_obj_time) / 1000.0;
		cur_dev->done =
		    ((float) cur_dev->remain_new) / cur_dev->prate;

	}
	free(dstats);
}

static void
update_device_queue(search_context_t * sc)
{
	device_handle_t *cur_dev;
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN)
			continue;
		device_set_limit(cur_dev->dev_handle, sc->dev_queue_limit);
	}
}
/*
 * helper function that write the value passed in the data to the
 * uint32_t that the cookie points to.
 */
static int
dctl_write_dev_queue(void *cookie, int len, char *data)
{
	search_context_t * sc;
	assert(cookie != NULL);
	assert(data != NULL);

	if (len < sizeof(uint32_t)) {
		return (ENOMEM);
	}

	sc = (search_context_t *)cookie;
	sc->dev_queue_limit = *(int *) data;

	update_device_queue(sc);
	return (0);
}

static int
dctl_read_dev_queue(void *cookie, int *len, char *data)
{
	search_context_t * sc;

	if (*len < sizeof(uint32_t)) {
		return (ENOMEM);
	}

	sc = (search_context_t *)cookie;

        *len = sizeof(uint32_t);
	*(uint32_t *) data = sc->dev_queue_limit;

	return (0);
}






static void
update_total_rate(search_context_t * sc)
{
	device_handle_t *cur_dev;
	float           min_done = 1000000.0;
	float           max_delta = 0;

	update_dev_stats(sc);

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		if (cur_dev->done < min_done) {
			min_done = cur_dev->done;
		}
	}

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		cur_dev->delta = cur_dev->done - min_done;
		if (cur_dev->delta < 0.0) {
			cur_dev->delta = 0.0;
		}
		if (cur_dev->delta > max_delta) {
			max_delta = cur_dev->delta;
		}
	}

	/*
	 * now adjust all the values 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		cur_dev->credit_incr =
		    (int) ((cur_dev->delta / max_delta) * MAX_CREDIT_INCR);
		if (cur_dev->credit_incr > MAX_CREDIT_INCR) {
			cur_dev->credit_incr = MAX_CREDIT_INCR;
		} else if (cur_dev->credit_incr < 1) {
			cur_dev->credit_incr = 1;
		}
	}
}


static void
update_delta_rate(search_context_t * sc)
{
	device_handle_t *cur_dev;
	float           min_done = 1000000.0;
	float           max_delta = 0;
	int             target;
	float           scale;

	update_dev_stats(sc);

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		if (cur_dev->done < min_done) {
			min_done = cur_dev->done;
		}
	}
	if (min_done == 0.0) {
		min_done = 1.0;
	}

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		cur_dev->delta = ((float) (cur_dev->done - min_done)) /
		    (float) min_done;
		if (cur_dev->delta < 0.0) {
			cur_dev->delta = 0.0;
		}
		if (cur_dev->delta > max_delta) {
			max_delta = cur_dev->delta;
		}
	}

	if (max_delta == 0) {
		max_delta = 1.0;
	}

	scale = (float) MAX_CREDIT_INCR / (float) max_delta;

	/*
	 * now adjust all the values 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		target = (int) (cur_dev->delta * scale);
		if (target > MAX_CREDIT_INCR) {
			target = MAX_CREDIT_INCR;
		} else if (target < 1) {
			target = 1;
		}
		if (target > cur_dev->credit_incr) {
			cur_dev->credit_incr++;
		} else if (target < cur_dev->credit_incr) {
			cur_dev->credit_incr--;
		}
	}
}

static void
update_rail(search_context_t * sc)
{
	device_handle_t *cur_dev;
	float           max_done = 0.0;
	float           target;

	update_dev_stats(sc);

	/*
	 * find the one that will finish the latest 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		if (cur_dev->done > max_done) {
			max_done = cur_dev->done;
		}
	}

	/*
	 * now adjust all the values 
	 */
	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		if (cur_dev->done == max_done) {
			target = MAX_CREDIT_INCR;
		} else {
			target = 1.0;
		}
		if (target > (float) cur_dev->credit_incr) {
			cur_dev->credit_incr++;
		} else if (target < (float) cur_dev->credit_incr) {
			cur_dev->credit_incr--;
		}
	}
}

static void
update_rates(search_context_t * sc)
{
	// printf("update rates \n");

	switch (sc->bg_credit_policy) {
	case CREDIT_POLICY_RAIL:
		update_rail(sc);
		break;

	case CREDIT_POLICY_PROP_TOTAL:
		update_total_rate(sc);
		break;

	case CREDIT_POLICY_PROP_DELTA:
		update_delta_rate(sc);
		break;

	case CREDIT_POLICY_STATIC:
	default:
		break;
	}
}

static void
refill_credits(search_context_t * sc)
{
	device_handle_t *cur_dev;

	for (cur_dev = sc->dev_list; cur_dev != NULL; cur_dev = cur_dev->next) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			continue;
		}
		cur_dev->cur_credits += (float) cur_dev->credit_incr;
		if (cur_dev->cur_credits > (float) MAX_CUR_CREDIT) {
			cur_dev->cur_credits = (float) MAX_CUR_CREDIT;
		}
	}
}

/*
 * XXX constant config 
 */
#define         POLL_SECS       1
#define         POLL_USECS      0


static obj_info_t     *
get_next_object(search_context_t * sc)
{
	device_handle_t *cur_dev;
	obj_info_t     *obj_inf;
	int             loop = 0;

	if (sc->last_dev == NULL) {
		cur_dev = sc->dev_list;
	} else {
		cur_dev = sc->last_dev->next;
	}

redo:
	while (cur_dev != NULL) {
		if (cur_dev->flags & DEV_FLAG_DOWN) {
			cur_dev = cur_dev->next;
			continue;
		}
		if (cur_dev->cur_credits > 0.0) {
			obj_inf = device_next_obj(cur_dev->dev_handle);
			if (obj_inf != NULL) {
				cur_dev->serviced++;
				sc->last_dev = cur_dev;
				return (obj_inf);
			}
		}
		cur_dev = cur_dev->next;
	}

	/*
	 * if we fall through and it is our first iteration then retry from
	 * the begining.  On the second interation we decide there isn't any
	 * data. 
	 */
	if (loop == 0) {
		loop = 1;
		cur_dev = sc->dev_list;
		goto redo;
	} else if (loop == 1) {
		loop = 2;
		cur_dev = sc->dev_list;
		refill_credits(sc);
		goto redo;
	}
	return (NULL);
}

static void
dec_object_credit(search_context_t * sc, double etime)
{
	double          credit = etime / sc->avg_proc_time;
	if (sc->last_dev != NULL) {
		sc->last_dev->cur_credits -= credit;
		// printf("dec_credits: id=%08x new %f delta %f \n",
		// sc->last_dev->dev_id,
		// sc->last_dev->cur_credits, credit);
	}
}



static int
bg_val(void *cookie)
{
	return (1);
}

/*
 * The main loop that the background thread runs to process
 * the data coming from the individual devices.
 */

static void    *
bg_main(void *arg)
{
	obj_data_t     *new_obj;
	obj_info_t     *obj_info;
	search_context_t *sc;
	int             err;
	bg_cmd_data_t  *cmd;
	int             any;
	int             active;
	device_handle_t *cur_dev;
	double          etime;
	struct timeval  this_time;
	struct timeval  next_time = { 0, 0 };
	struct timezone tz;
	struct timespec timeout;
	uint32_t        loop_count = 0;
	uint32_t        dummy = 0;
	uint32_t		exec_mode_thresh_low = MAXINT;
	uint32_t		exec_mode_thresh_high = MAXINT;

	sc = (search_context_t *) arg;

	err = dctl_register_node(HOST_PATH, HOST_BACKGROUND);
	assert(err == 0);
	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "loop_count",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &loop_count);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "cpu_split",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &do_cpu_update);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "dev_queue_max",
				 DCTL_DT_UINT32, dctl_read_dev_queue,
				 dctl_write_dev_queue, sc);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "pend_queue_max",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &sc->pend_lw);
	assert(err == 0);


	err =
	    dctl_register_leaf(HOST_BACKGROUND_PATH, "dummy", DCTL_DT_UINT32,
			       dctl_read_uint32, dctl_write_uint32, &dummy);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "credit_policy",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &sc->bg_credit_policy);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "exec_mode_thresh_lo",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &exec_mode_thresh_low);
	assert(err == 0);

	err = dctl_register_leaf(HOST_BACKGROUND_PATH, "exec_mode_thresh_hi",
				 DCTL_DT_UINT32, dctl_read_uint32,
				 dctl_write_uint32, &exec_mode_thresh_high);
	assert(err == 0);


	sc->avg_proc_time = 0.01;

	/*
	 * There are two main tasks that this thread does. The first
	 * is to look for commands from the main process that tell
	 * what to do.  The commands are stored in the "bg_ops" ring.
	 * Typical commands are to load searchlet, stop search, etc.
	 *
	 * The second task is to objects that have arrived from
	 * the storage devices and finish processing them. The
	 * incoming objects are in the "unproc_ring".  After
	 * processing they are released or placed into the "proc_ring"
	 * ring bsed on the results of the evaluation.
	 */

	while (1) {
		loop_count++;
		any = 0;
		/*
		 * This code processes the objects that have not yet
		 * been fully processed.
		 */
		if ((sc->bg_status & BG_STARTED) &&
		    (ring_count(sc->proc_ring) < sc->pend_lw)) {
			obj_info = get_next_object(sc);
			if (obj_info != NULL) {
				any = 1;
				new_obj = obj_info->obj;
				/*
				 * Make sure the version number is the
				 * latest.  If it is not equal, then this
				 * is probably data left over from a previous
				 * search that is working its way through
				 * the system.
				 */
				if (sc->cur_search_id != obj_info->ver_num) {
					ls_release_object(sc, new_obj);
					free(obj_info);
					continue;
				}

				/*
				 * Now that we have an object, go ahead
				 * an evaluate all the filters on the
				 * object.
				 */
				err = eval_filters(new_obj, sc->bg_fdata, 1,
						   &etime, &sc, bg_val, NULL);
				sc->avg_proc_time =
				    (0.90 * sc->avg_proc_time) +
				    (0.10 * etime);
				dec_object_credit(sc, etime);
				if (err == 0) {
					ls_release_object(sc, new_obj);
					free(obj_info);
				} else {
					err = ring_enq(sc->proc_ring, (void *) obj_info);

					if (err) {
						/*
						 * XXX handle overflow
						 * gracefully !!! 
						 */
						/*
						 * XXX log 
						 */
						assert(0);
					}

					sc->host_stats.hs_objs_queued++;

				    /*
				     * change filter execution modes based on the
				     * current mode and the number of objects in 
				     * the proc_ring.
				     */
					int proc_ring_count = ring_count(sc->proc_ring);
					int new_mode = sc->search_exec_mode;
					
				    log_message(LOGT_BG, LOGL_DEBUG, 
				    			"Search %d, %d objects queued for app, limit %d", 
				    			sc->cur_search_id, 
				    			proc_ring_count,
				    			sc->dev_queue_limit);
				    
				    if (sc->search_exec_mode == FM_CURRENT) {
				    	if (proc_ring_count > exec_mode_thresh_high) {
				    		new_mode = FM_MODEL;
				    	} else if (proc_ring_count > exec_mode_thresh_low) {
				    		new_mode = FM_HYBRID;
				    	} 
					} else if (sc->search_exec_mode == FM_HYBRID) {
				    	if (proc_ring_count <= exec_mode_thresh_low) {
				    		new_mode = FM_CURRENT;
				    	} else if (proc_ring_count > exec_mode_thresh_high) {
				    		new_mode = FM_MODEL;
				    	} 
				    } else if (sc->search_exec_mode == FM_MODEL) {
				    	if (proc_ring_count <= exec_mode_thresh_low) {
				    		new_mode = FM_CURRENT;
				    	} else if (proc_ring_count <= exec_mode_thresh_high) {
				    		new_mode = FM_HYBRID;
				    	} 
				    }
				    
				    if (sc->search_exec_mode != new_mode) {			    
						for (cur_dev = sc->dev_list; cur_dev != NULL;
					    	 cur_dev = cur_dev->next) {
					    	device_set_exec_mode(cur_dev->dev_handle, 
					    					sc->cur_search_id, new_mode);
					    }
				    }
				}
			} else {
				/*
				 * These are no objects.  See if all the devices
				 * are done.
				 */

				active = 0;
				for (cur_dev = sc->dev_list; cur_dev != NULL;
				    cur_dev = cur_dev->next) {
					if (cur_dev->flags & DEV_FLAG_DOWN) {
						continue;
					}
					if ((cur_dev->
					     flags & DEV_FLAG_COMPLETE) == 0) {
						active = 1;
						break;
					}
				}

				if ((active == 0)
				    && (sc->cur_status == SS_ACTIVE)) {
					sc->cur_status = SS_DONE;
				}
			}
		} else {
			/*
			 * There are no objects.  See if all devices
			 * are done.
			 */
		}

		/*
		 * timeout look that runs once a second 
		 */
		gettimeofday(&this_time, &tz);

		if (((this_time.tv_sec == next_time.tv_sec) &&
		     (this_time.tv_usec >= next_time.tv_usec)) ||
		    (this_time.tv_sec > next_time.tv_sec)) {

			update_rates(sc);

			assert(POLL_USECS < 1000000);
			next_time.tv_sec = this_time.tv_sec + POLL_SECS;
			next_time.tv_usec = this_time.tv_usec + POLL_USECS;

			if (next_time.tv_usec >= 1000000) {
				next_time.tv_usec -= 1000000;
				next_time.tv_sec += 1;
			}
		}

		/*
		 * This section looks for any commands on the bg ops 
		 * rings and processes them.
		 */
		cmd = (bg_cmd_data_t *) ring_deq(sc->bg_ops);
		if (cmd != NULL) {
			any = 1;
			switch (cmd->cmd) {
			case BG_SPEC:
				sc->bg_status |= BG_SET_SEARCHLET;
				err = fexec_load_spec(&sc->bg_fdata,
				    &cmd->spec_sig);
				assert(!err);
				break;
			case BG_LIB:
				err = fexec_load_obj(sc->bg_fdata,
				    &cmd->lib_sig);
				assert(!err);
				break;


			case BG_SET_BLOB:
				fexec_set_blob(sc->bg_fdata, cmd->filter_name,
					       cmd->blob_len, cmd->blob);
				assert(!err);
				break;

			case BG_START:
				/*
				 * XXX reinit filter is not new one 
				 */
				if (!(sc->bg_status & BG_SET_SEARCHLET)) {
					printf("start: no searchlet\n");
					break;
				}
				/*
				 * XXX clear out the proc ring 
				 */
				{
					obj_data_t     *new_obj;
					obj_info_t     *obj_info;

					while (!ring_empty(sc->proc_ring)) {
						/*
						 * XXX lock 
						 */
						obj_info =
						    (obj_info_t *)
						    ring_deq(sc->proc_ring);
						new_obj = obj_info->obj;
						ls_release_object(sc,
								  new_obj);
						free(obj_info);
					}
				}

				/*
				 * XXX clean up any stats ?? 
				 */

				fexec_init_search(sc->bg_fdata);
				sc->bg_status |= BG_STARTED;
				break;

			case BG_STOP:
				sc->bg_status &= ~BG_STARTED;
				/*
				 * XXX toher state ?? 
				 */
				fexec_term_search(sc->bg_fdata);
				break;

			default:
				printf("background !! \n");
				break;

			}
			free(cmd);
		}

		if (any == 0) {
			timeout.tv_sec = 0;
			timeout.tv_nsec = 10000000;	/* 10 ms */
			nanosleep(&timeout, NULL);
		}
	}
}

/*
 * This function sets the searchlet for the background thread to use
 * for processing the data.  This function doesn't actually load
 * the searchlet, puts a command into the queue for the background
 * thread to process that points to the new searchlet.  
 *
 * Having a single thread processing the real operations avoids
 * any ordering/deadlock/etc. problems that may arise. 
 *
 * XXX TODO:  we need to copy the thread into local memory.
 *            we need to test the searchlet so we can fail synchronously. 
 */

int
bg_set_spec(search_context_t * sc, int id, sig_val_t *sig)
{
	bg_cmd_data_t  *cmd;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *) malloc(sizeof(*cmd));
	assert(cmd != NULL);

	cmd->cmd = BG_SPEC;
	cmd->ver_id = (bg_op_type_t) id;
	memcpy(&cmd->spec_sig, sig, sizeof(*sig));
	ring_enq(sc->bg_ops, (void *) cmd);
	return (0);
}

int
bg_set_lib(search_context_t * sc, int id, sig_val_t *sig)
{
	bg_cmd_data_t  *cmd;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *) malloc(sizeof(*cmd));
	assert(cmd != NULL);

	cmd->cmd = BG_LIB;
	cmd->ver_id = (bg_op_type_t) id;
	memcpy(&cmd->lib_sig, sig, sizeof(*sig));
	ring_enq(sc->bg_ops, (void *) cmd);
	return (0);
}


int
bg_start_search(search_context_t * sc, int id)
{
	bg_cmd_data_t  *cmd;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		/*
		 * XXX log 
		 */
		/*
		 * XXX error ? 
		 */
		return (0);
	}

	cmd->cmd = BG_START;
	ring_enq(sc->bg_ops, (void *) cmd);
	return (0);
}

int
bg_stop_search(search_context_t * sc, int id)
{
	bg_cmd_data_t  *cmd;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		/*
		 * XXX log 
		 */
		/*
		 * XXX error ? 
		 */
		return (0);
	}
	cmd->cmd = BG_STOP;
	ring_enq(sc->bg_ops, (void *) cmd);
	return (0);
}

int
bg_set_blob(search_context_t * sc, int id, char *filter_name,
	    int blob_len, void *blob_data)
{
	bg_cmd_data_t  *cmd;
	void           *new_blob;

	/*
	 * Allocate a command struct to store the new command.
	 */
	cmd = (bg_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		/*
		 * XXX log 
		 */
		/*
		 * XXX error ? 
		 */
		return (0);
	}

	new_blob = malloc(blob_len);
	assert(new_blob != NULL);
	memcpy(new_blob, blob_data, blob_len);


	cmd->cmd = BG_SET_BLOB;
	/*
	 * XXX local storage for these !!! 
	 */
	cmd->filter_name = strdup(filter_name);
	assert(cmd->filter_name != NULL);

	cmd->blob_len = blob_len;
	cmd->blob = new_blob;


	ring_enq(sc->bg_ops, (void *) cmd);
	return (0);
}


/*
 *  This function intializes the background processing thread that
 *  is used for taking data ariving from the storage devices
 *  and completing the processing.  This thread initializes the ring
 *  that takes incoming data.
 */

int
bg_init(search_context_t * sc, int id)
{
	int             err;
	pthread_t       thread_id;

	/*
	 * Initialize the ring of commands for the thread.
	 */
	err = ring_init(&sc->bg_ops, BG_RING_SIZE);
	if (err) {
		/*
		 * XXX err log 
		 */
		return (err);
	}

	/*
	 * Create a thread to handle background processing.
	 */
	err = pthread_create(&thread_id, NULL, bg_main, (void *) sc);
	if (err) {
		/*
		 * XXX log 
		 */
		printf("failed to create background thread \n");
		return (ENOENT);
	}
	return (0);
}
