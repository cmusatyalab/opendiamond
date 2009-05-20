/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2008 Carnegie Mellon University
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
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <netdb.h>
#include <glib.h>
#include "sig_calc.h"
#include "diamond_consts.h"
#include "diamond_types.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "dctl_impl.h"
#include "lib_sstub.h"
#include "lib_dconfig.h"
#include "dconfig_priv.h"
#include "lib_log.h"
#include "rcomb.h"
#include "lib_filterexec.h"
#include "search_state.h"
#include "dctl_common.h"
#include "lib_ocache.h"
#include "sys_attr.h"
#include "obj_attr.h"
#include "odisk_priv.h"


#define	SAMPLE_TIME_FLOAT	0.2
#define	SAMPLE_TIME_NANO	200000000

static void    *update_bypass(void *arg);

static int reexec_active = 0;
pthread_cond_t reexec_done = PTHREAD_COND_INITIALIZER;
pthread_mutex_t reexec_active_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t object_eval_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * XXX other place 
 */
extern int      do_cleanup;
extern int      do_fork;
extern int 		do_background;
extern int      active_searches;
extern int      idle_background;
extern pid_t    background_pid;

int	background_search;

static int      search_free_obj(search_state_t * sstate, obj_data_t * obj);

typedef enum {
	DEV_STOP,
	DEV_TERM,
	DEV_START,
	DEV_SPEC,
	DEV_OBJ,
	DEV_BLOB
} dev_op_type_t;


typedef struct {
	char           *fname;
	void           *blob;
	int             blen;
} dev_blob_data_t;

typedef struct {
	dev_op_type_t   cmd;
	sig_val_t	sig;
	union {
		dev_blob_data_t bdata;
		host_stats_t    hdata;
	} extra_data;
} dev_cmd_data_t;

int
search_stop(void *app_cookie, host_stats_t *hstats)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;

	log_message(LOGT_DISK, LOGL_TRACE, "search_stop");

	sstate = (search_state_t *) app_cookie;
	sstate->user_state = USER_UNKNOWN;
	
	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_STOP;
	cmd->extra_data.hdata = *hstats;

	g_async_queue_push(sstate->control_ops, cmd);
	return (0);
}


int
search_term(void *app_cookie)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;

	log_message(LOGT_DISK, LOGL_TRACE, "search_stop");

	sstate = (search_state_t *) app_cookie;
	sstate->user_state = USER_UNKNOWN;
	
	/*
	 * Allocate a new command and put it onto the ring
	 * of commands being processed.
	 */
	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}
	cmd->cmd = DEV_TERM;

	/*
	 * Put it on the ring.
	 */
	g_async_queue_push(sstate->control_ops, cmd);
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
	return (0);
}



int
search_start(void *app_cookie)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;

	/*
	 * XXX start 
	 */
	log_message(LOGT_DISK, LOGL_TRACE, "search_start");

	sstate = (search_state_t *) app_cookie;
	sstate->user_state = USER_WAITING;
	
	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}
	cmd->cmd = DEV_START;

	g_async_queue_push(sstate->control_ops, cmd);

	return (0);
}


/*
 * This is called to set the searchlet for the current search.
 */

int
search_set_spec(void *app_cookie, sig_val_t *spec_sig)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;

	char *sig_str = sig_string(spec_sig);

	log_message(LOGT_DISK, LOGL_TRACE, "search_set_spec: %s", sig_str);
	free(sig_str);

	sstate = (search_state_t *) app_cookie;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_SPEC;

	memcpy(&cmd->sig, spec_sig, sizeof(*spec_sig));
	g_async_queue_push(sstate->control_ops, cmd);
	return (0);
}


int
search_set_obj(void *app_cookie, sig_val_t * objsig)
{
	dev_cmd_data_t *cmd;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	cmd = (dev_cmd_data_t *) malloc(sizeof(*cmd));
	if (cmd == NULL) {
		return (1);
	}

	cmd->cmd = DEV_OBJ;

	memcpy(&cmd->sig, objsig, sizeof(*objsig));
	g_async_queue_push(sstate->control_ops, cmd);
	return (0);
}

/*
 * Reset the statistics for the current search.
 */
static void
clear_ss_stats(search_state_t * sstate)
{
	sstate->obj_total = 0;
	sstate->obj_processed = 0;
	sstate->obj_dropped = 0;
	sstate->obj_passed = 0;
	sstate->obj_skipped = 0;
	sstate->obj_bg_processed = 0;
	sstate->obj_bg_dropped = 0;
	sstate->obj_bg_passed = 0;
	sstate->network_stalls = 0;
	sstate->tx_full_stalls = 0;
	sstate->tx_idles = 0;

	sstate->avg_ratio = 0.0;
	sstate->avg_int_ratio = 0;
	sstate->old_proc = 0;
}


static void
sstats_drop(void *cookie)
{
	search_state_t *sstate = (search_state_t *) cookie;
	sstate->obj_dropped++;
}


static void
sstats_process(void *cookie)
{
	search_state_t *sstate = (search_state_t *) cookie;
	sstate->obj_processed++;
}

/*
 * Take the current command and process it.  Note, the command
 * will be free'd by the caller.
 */
static void
dev_process_cmd(search_state_t * sstate, dev_cmd_data_t * cmd)
{
	int             err;
	query_info_t	qinfo;

	switch (cmd->cmd) {
	case DEV_STOP:
		/*
		 * Stop the current search 
		 */
		sstate->flags &= ~DEV_FLAG_RUNNING;
		err = odisk_flush(sstate->ostate);
		assert(err == 0);

		/* delay cleaning up the filter exec state */
		//ceval_stop(sstate->fdata);
		//fexec_term_search(sstate->fdata);

		/*
		 * flush objects in the transmit queue 
		 */
		err = sstub_flush_objs(sstate->comm_cookie);
		assert(err == 0);

		/*
		 * dump search statistics 
		 */
		log_message(LOGT_DISK, LOGL_INFO, 
					"object stats: dev processed %d passed %d dropped %d", 
					sstate->obj_processed,
					sstate->obj_passed,
					sstate->obj_dropped); 
		log_message(LOGT_DISK, LOGL_INFO,
					"\thost objs received %d queued %d read %d",
					cmd->extra_data.hdata.hs_objs_received,
					cmd->extra_data.hdata.hs_objs_queued,
					cmd->extra_data.hdata.hs_objs_read);
		log_message(LOGT_DISK, LOGL_INFO,
					"\tapp objs queued %d presented %d",
					cmd->extra_data.hdata.hs_objs_uqueued,
					cmd->extra_data.hdata.hs_objs_upresented);
		break;

	case DEV_TERM:
		break;

	case DEV_START:
		/*
		 * Start the emulated device for now.
		 * XXX do this for real later.
		 */

		/* clean up any search state from a previous run */
		if (sstate->fdata) {
		    ceval_stop(sstate->fdata);
		    fexec_term_search(sstate->fdata);
		}

		/*
		 * Clear the stats.
		 */
		clear_ss_stats(sstate);

		/*
		 * JIAYING: for now, we calculate the signature for the
		 * whole library and spec file 
		 */
		qinfo.session = sstate->cinfo;
		ceval_init_search(sstate->fdata, &qinfo, sstate->cstate);

		// LBM - odisk group
		err = odisk_reset(sstate->ostate);
		if (err) {
			/*
			 * XXX log 
			 */
			/*
			 * XXX crap !! 
			 */
			return;
		}
		err = ocache_start();
		if (err) {
			return;
		}

		/*
		 * init the filter exec code 
		 */
		fexec_init_search(sstate->fdata);
		err = ceval_start(sstate->fdata);
		if (err) {
			return;
		}

		sstate->obj_total = odisk_get_obj_cnt(sstate->ostate);
		sstate->flags |= DEV_FLAG_RUNNING;
		break;

	case DEV_SPEC:
		err = fexec_load_spec(&sstate->fdata, &cmd->sig);
		if (err) {
			/*
			 * XXX log 
			 */
			assert(0);
			return;
		}
		break;

	case DEV_OBJ:
		err = fexec_load_obj(sstate->fdata, &cmd->sig);
		if (err) {
			/*
			 * XXX log 
			 */
			assert(0);
			return;
		}
		break;

	case DEV_BLOB:{
			char           *name;
			int             blen;
			void           *blob;

			name = cmd->extra_data.bdata.fname;
			blen = cmd->extra_data.bdata.blen;
			blob = cmd->extra_data.bdata.blob;

			err = fexec_set_blob(sstate->fdata, name, blen, blob);
			assert(err == 0);

			free(name);
			free(blob);
			break;
	}

	default:
		printf("unknown command %d \n", cmd->cmd);
		break;

	}
}


#ifdef	XXX
static void
dynamic_update_bypass(search_state_t * sstate)
{
	int             err;
	float           avg_cost;

	err = fexec_estimate_cost(sstate->fdata, sstate->fdata->fd_perm,
				  1, 0, &avg_cost);
	if (err) {
		avg_cost = 30000000.0;
	}

	if (sstate->obj_processed != 0) {
		float           ratio;
		float           new_val;

		ratio =
		    (float) sstate->old_proc / (float) sstate->obj_processed;
		new_val = sstate->avg_ratio * ratio;
		new_val += sstate->split_ratio * (1 - ratio);
		sstate->avg_ratio = new_val;
		sstate->avg_int_ratio = (int) new_val;
		sstate->smoothed_ratio = 0.5 * sstate->smoothed_ratio +
		    0.5 * new_val;
		sstate->smoothed_int_ratio = (int) sstate->smoothed_ratio;
		sstate->old_proc = sstate->obj_processed;
	}

	sstate->split_ratio = (int) ((sstate->pend_compute *
				      (float) sstate->split_mult));

	if (sstate->split_ratio < 5) {
		sstate->split_ratio = 5;
	}
	if (sstate->split_ratio > 100) {
		sstate->split_ratio = 100;
	}
}
#else

static float
deq_beta(float proc_rate, float erate)
{
	float           beta;
	if ((proc_rate == 0.0) || (erate < 0.00001)) {
		return (0.5);
	}
	beta = proc_rate / erate;
	return (beta);
}

static float
enq_beta(float proc_rate, float drate, int pend_objs)
{
	float           beta;
	float           target;

	if ((proc_rate == 0.0) || (drate < 0.00001)) {
		return (0.5);
	}

	target = ((drate * SAMPLE_TIME_FLOAT) +
		  ((float) (20 - pend_objs))) / SAMPLE_TIME_FLOAT;
	if (target < 0.0) {
		target = 0;
	}
	beta = 1.0 / ((target / proc_rate) + 1);

	return (beta);
}


static void
dynamic_update_bypass(search_state_t * sstate)
{
	int             err;
	float           avg_cost;
	float           betain;
	float           betaout;
	float           proc_rate;
	float           erate;
	float           drate;

	err = fexec_estimate_cur_cost(sstate->fdata, &avg_cost);
	if (err) {
		avg_cost = 30000000.0;
	}

	if (sstate->obj_processed != 0) {
		float           ratio;
		float           new_val;

		ratio =
		    (float) sstate->old_proc / (float) sstate->obj_processed;
		new_val = sstate->avg_ratio * ratio;
		new_val += sstate->split_ratio * (1 - ratio);
		sstate->avg_ratio = new_val;
		sstate->avg_int_ratio = (int) new_val;
		sstate->smoothed_ratio = 0.5 * sstate->smoothed_ratio +
		    0.5 * new_val;
		sstate->smoothed_int_ratio = (int) sstate->smoothed_ratio;
		sstate->old_proc = sstate->obj_processed;
	}

	drate = sstub_get_drate(sstate->comm_cookie);
	erate = odisk_get_erate(sstate->ostate);
	proc_rate = fexec_get_prate(sstate->fdata);


	betain = deq_beta(proc_rate, erate);
	betaout = enq_beta(proc_rate, drate, sstate->pend_objs);

	// printf("betain %f betaout %f drate %f erate %f prate %f\n",
	// betain, betaout,
	// drate, erate, proc_rate);
	if (betain > betaout) {
		sstate->split_ratio = (int) (betain * 100.0);
	} else {
		sstate->split_ratio = (int) (betaout * 100.0);
	}

	if (sstate->split_ratio < 5) {
		sstate->split_ratio = 5;
	}
	if (sstate->split_ratio > 100) {
		sstate->split_ratio = 100;
	}
}
#endif

static void    *
update_bypass(void *arg)
{
	search_state_t *sstate = (search_state_t *) arg;
	uint            old_target;
	float           ratio;
	struct timespec ts;

	while (1) {
		if (sstate->flags & DEV_FLAG_RUNNING) {
			switch (sstate->split_type) {
			case SPLIT_TYPE_FIXED:
				ratio = ((float) sstate->split_ratio) / 100.0;
				fexec_update_bypass(sstate->fdata, ratio);
				fexec_update_grouping(sstate->fdata, ratio);
				break;

			case SPLIT_TYPE_DYNAMIC:
				old_target = sstate->split_ratio;
				dynamic_update_bypass(sstate);
				ratio = ((float) sstate->split_ratio) / 100.0;
				fexec_update_bypass(sstate->fdata, ratio);
				fexec_update_grouping(sstate->fdata, ratio);
				break;
			}
		}
		ts.tv_sec = 0;
		ts.tv_nsec = SAMPLE_TIME_NANO;
		nanosleep(&ts, NULL);
	}
}

/*
 * This function is called to see if we should continue
 * processing an object, or put it into the queue.
 */
static int
continue_fn(void *cookie)
{
	search_state_t *sstate = cookie;
#ifdef	XXX

	float           avg_cost;
	int             err;
	err = fexec_estimate_cost(sstate->fdata, sstate->fdata->fd_perm,
				  1, 0, &avg_cost);
	if (err) {
		avg_cost = 30000000.0;
	}

	/*
	 * XXX include input queue size 
	 */
	if ((int) (sstate->pend_compute / avg_cost) < sstate->split_bp_thresh) {
		return (0);
	} else {
		return (1);
	}
#else
	if ((sstate->pend_objs < 4)
	    && (odisk_num_waiting(sstate->ostate) > 4)) {
		return (0);
	} else {
		return (2);
	}
#endif
}


typedef struct {
	int	max_names;
	int	num_names;
	char ** nlist;
} good_objs_t;


static void
init_good_objs(good_objs_t *gobj)
{
	gobj->num_names = 0;
	gobj->max_names = 256;

	gobj->nlist = malloc(sizeof(char *) * gobj->max_names);
	assert(gobj->nlist != NULL); 
}

static void
clear_good_objs(good_objs_t *gobj)
{
	int	i;

	for (i=0; i < gobj->num_names; i++)
		free(gobj->nlist[i]);

	gobj->num_names = 0;
}

/*
 * Helper function to save hits.
 */ 

static void
save_good_name(good_objs_t *gobj, obj_data_t *obj)
{
	size_t          size;
	int             err;
	unsigned char  *name;
	

	if (gobj->num_names == gobj->max_names) {
		gobj->max_names += 256;
		gobj->nlist = realloc(gobj->nlist, 
		    sizeof(char *) * gobj->max_names);
		assert(gobj->nlist != NULL);
	}

	err = obj_ref_attr(&obj->attr_info, DISPLAY_NAME, &size, &name);
	if (err) {
		fprintf(stdout, "name Unknown \n");
	} else {
		gobj->nlist[gobj->num_names] = strdup((char *)name);
		assert(gobj->nlist[gobj->num_names] != NULL);
		gobj->num_names++;
	}
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
	int             err;
	int             any;
	int             lookahead = 0;
	int             complete;
	struct timespec timeout;
	int             force_eval;
	int				pass;
	good_objs_t		gobj;
	query_info_t	qinfo;
	pid_t		wait_pid;
	int		wait_status;
	double          elapsed;

	sstate = (search_state_t *) arg;

	init_good_objs(&gobj);

	log_message(LOGT_DISK, LOGL_DEBUG, "adiskd: device_main: 0x%x", arg);

	/*
	 * XXX need to open comm channel with device
	 */
	while (1) {
		any = 0;
		cmd = (dev_cmd_data_t *) g_async_queue_try_pop(sstate->control_ops);
		if (cmd != NULL) {
			any = 1;
			dev_process_cmd(sstate, cmd);
			free(cmd);
		}

		if (sstate->pend_compute >= sstate->pend_max) {
		}

		/* allow reexecution to preempt the background processing */
		pthread_mutex_lock(&reexec_active_mutex);
		while (reexec_active)
		    pthread_cond_wait(&reexec_done, &reexec_active_mutex);
		pthread_mutex_unlock(&reexec_active_mutex);

		/* odisk_load_obj & ceval_filters2 cannot be run
		 * concurrently */
		pthread_mutex_lock(&object_eval_mutex);
		/*
		 * XXX look for data from device to process.
		 */
		if (!(sstate->flags & DEV_FLAG_RUNNING)) {
			clear_good_objs(&gobj);
		}
		else if (sstate->pend_objs < sstate->pend_max) {
			/*
			 * If we ever did lookahead to heat the cache
			 * we now inject those names back into the processing
			 * stages. If the lookahead was from a previous
			 * search we will just clean up otherwise tell
			 * the cache eval code about the objects.
			 */
			if (lookahead) {
				ceval_inject_names(gobj.nlist, gobj.num_names);
				init_good_objs(&gobj);
				lookahead = 0;
			}
			force_eval = 0;
			err = odisk_next_obj(&new_obj, sstate->ostate);

			/*
			 * If no fresh objects, try to get some from
			 * the partial queue.
			 */
			if (err == ENOENT) {
				err = sstub_get_partial(sstate->comm_cookie,
						      &new_obj);
				if (err == 0) {
					force_eval = 1;
					sstate->pend_objs--;
					sstate->pend_compute -=
					    new_obj->remain_compute;
				} else {
					err = ENOENT;
				}
			}
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
						     new_obj, 1);
				if (err) {
					/*
					 * XXX overflow gracefully  and log
					 */
				} else {
					/*
					 * XXX log 
					 */
					sstate->pend_objs++;
				}
			} else if (err) {
				printf("read error \n");
				/*
				 * printf("dmain: failed to get obj !! \n"); 
				 */
				/*
				 * sleep(1); 
				 */
				pthread_mutex_unlock(&object_eval_mutex);
				continue;
			} else {
				any = 1;
				sstate->obj_processed++;

				/*
				 * We want to process some number of objects
				 * locally to get the necessary statistics.  
				 * Setting force_eval will make sure process 
				 * the whole object.
				 */

				if ((sstate->obj_processed & 0xf) == 0xf) {
					force_eval = 1;
				}

				/*
				 * XXX anomaly detection stuff, not in right place
				 */
				new_obj->session_variables_state = sstate->session_variables_state;

				qinfo.session = sstate->cinfo;
				pass = ceval_filters2(new_obj, sstate->fdata,
						      force_eval, &elapsed,
						      sstate->exec_mode,
						      &qinfo, sstate, continue_fn);

				if (pass == 0) {
					sstate->obj_dropped++;
					search_free_obj(sstate, new_obj);
				} else {
					sstate->obj_passed++;
					if (pass == 1) {
						complete = 0;
					} else {
						complete = 1;
					}

					sstate->pend_objs++;
					sstate->pend_compute +=
					    new_obj->remain_compute;

					err =sstub_send_obj(sstate->comm_cookie,
							     new_obj, complete);
					if (err) {
						/*
						 * XXX overflow gracefully 
						 */
						sstate->pend_objs--;
						sstate->pend_compute -=
						    new_obj->remain_compute;
					}
				}
			}
		}
		else if (sstate->work_ahead) {
			/* 
			 * If work ahead is enabled we continue working
			 * on the objects.  We keep track of the ones that
			 * pass so we can re-fetch them later.
			 */
			err = odisk_next_obj(&new_obj, sstate->ostate);
			if (err == 0) {
				any = 1;	
				lookahead = 1;

				/*
				 * XXX anomaly detection stuff, not in right place
				 */
				new_obj->session_variables_state = sstate->session_variables_state;

				qinfo.session = sstate->cinfo;
				pass = ceval_filters2(new_obj, sstate->fdata,
						      1, &elapsed,
						      sstate->exec_mode, &qinfo,
						      sstate, NULL);
				if (pass == 0) {
					sstate->obj_dropped++;
					sstate->obj_processed++;
				} else {
					save_good_name(&gobj, new_obj);
				}
				search_free_obj(sstate, new_obj);
			} else {
				sstate->tx_full_stalls++;
			}
		} else {
			sstate->tx_full_stalls++;
		}

		pthread_mutex_unlock(&object_eval_mutex);

		/*
		 * intra-search background processing - 
		 * clean up zombies from earlier processing
		 */
		if (background_pid != -1) {
			wait_pid = waitpid(-1, &wait_status, WNOHANG | WUNTRACED);
			if (wait_pid > 0) {
				if (wait_pid == background_pid) {
					background_pid = -1;
					background_search = 0;
				} 
			}
		}
		
		/*
		 * start a new background search if enabled
		 */
		if (do_background && background_search && (background_pid == -1)) {
			if (do_fork)  {
				background_pid = fork();
				if (background_pid == 0) {
					start_background();
					exit(0);
				}
			} else {
				start_background();
			}
		}

		/*
		 * If we didn't have any work to process this time around,
		 * then we sleep on a cond variable for a small amount
		 * of time.
		 */
		if (!any) {
			timeout.tv_sec = 0;
			timeout.tv_nsec = 10000000;	/* XXX 10ms */
			nanosleep(&timeout, NULL);
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
	pid_t			new_proc;

	/* kill any background processing if appropriate */
	if ((idle_background) && (background_pid != -1)) {
		kill(background_pid, SIGHUP);
	}

	/*
	 * We have a new connection decide whether to fork or not
	 */
	if (do_fork) {
		new_proc = fork();
    } else {
		new_proc = 0;
    }
	
	if (new_proc != 0) {
		active_searches++;
		return 1;
	}

	sstate = (search_state_t *) calloc(1, sizeof(*sstate));
	if (sstate == NULL) {
		*app_cookie = NULL;
		exit(1);
	}

	/*
	 * Set the return values to this "handle".
	 */
	*app_cookie = sstate;

	dctl_register_node(ROOT_PATH, SEARCH_NAME);

	dctl_register_u32(DEV_SEARCH_PATH, "work_ahead", O_RDWR,
			  &sstate->work_ahead);
	dctl_register_u32(DEV_SEARCH_PATH, "obj_total", O_RDONLY,
			  &sstate->obj_total);
	dctl_register_u32(DEV_SEARCH_PATH, "obj_processed", O_RDONLY,
			  &sstate->obj_processed);
	dctl_register_u32(DEV_SEARCH_PATH, "obj_dropped", O_RDONLY,
			  &sstate->obj_dropped);
	dctl_register_u32(DEV_SEARCH_PATH, "obj_pass", O_RDONLY,
			  &sstate->obj_passed);
	dctl_register_u32(DEV_SEARCH_PATH, "obj_skipped", O_RDONLY,
			  &sstate->obj_skipped);

	dctl_register_u32(DEV_SEARCH_PATH, "nw_stalls", O_RDONLY,
			  &sstate->network_stalls);
	dctl_register_u32(DEV_SEARCH_PATH, "tx_full_stalls", O_RDONLY,
			  &sstate->tx_full_stalls);
	dctl_register_u32(DEV_SEARCH_PATH, "tx_idles", O_RDONLY,
			  &sstate->tx_idles);

	dctl_register_u32(DEV_SEARCH_PATH, "pend_objs", O_RDONLY,
			  &sstate->pend_objs);
	dctl_register_u32(DEV_SEARCH_PATH, "pend_maximum", O_RDWR,
			  &sstate->pend_max);
	dctl_register_u32(DEV_SEARCH_PATH, "split_type", O_RDWR,
			  &sstate->split_type);
	dctl_register_u32(DEV_SEARCH_PATH, "split_ratio", O_RDWR,
			  &sstate->split_ratio);
	dctl_register_u32(DEV_SEARCH_PATH, "split_auto_step", O_RDWR,
			  &sstate->split_auto_step);
	dctl_register_u32(DEV_SEARCH_PATH, "split_bp_thresh", O_RDWR,
			  &sstate->split_bp_thresh);
	dctl_register_u32(DEV_SEARCH_PATH, "split_multiplier", O_RDWR,
			  &sstate->split_mult);
	dctl_register_u32(DEV_SEARCH_PATH, "average_ratio", O_RDONLY,
			  &sstate->avg_int_ratio);
	dctl_register_u32(DEV_SEARCH_PATH, "smoothed_beta", O_RDONLY,
			  &sstate->smoothed_int_ratio);

	dctl_register_node(ROOT_PATH, DEV_NETWORK_NODE);
	dctl_register_node(ROOT_PATH, DEV_FEXEC_NODE);
	dctl_register_node(ROOT_PATH, DEV_OBJ_NODE);
	dctl_register_node(ROOT_PATH, DEV_CACHE_NODE);

	/*
	 * Initialize the log for the new process
	 */
	log_init(LOG_PREFIX, DEV_SEARCH_PATH);

	sstub_get_conn_info(comm_cookie, &sstate->cinfo);

	char host[NI_MAXHOST], port[NI_MAXSERV];
	err = getnameinfo((struct sockaddr *)&sstate->cinfo.clientaddr,
			  sstate->cinfo.clientaddr_len,
			  host, sizeof(host), port, sizeof(port),
			  NI_NUMERICHOST | NI_NUMERICSERV);
	if (!err)
	    log_message(LOGT_DISK, LOGL_INFO,
			"adiskd: new connection received from %s:%s",host,port);
	else
	    log_message(LOGT_DISK, LOGL_ERR,
			"adiskd: failed to resolve peer: %s",gai_strerror(err));

	/*
	 * initialize libfilterexec
	 */
	fexec_system_init();

	/*
	 * init the ring to hold the queue of pending operations.
	 */
	sstate->control_ops = g_async_queue_new();

	sstate->flags = 0;
	sstate->comm_cookie = comm_cookie;

	sstate->work_ahead = SSTATE_DEFAULT_WORKAHEAD;

	sstate->pend_max = SSTATE_DEFAULT_PEND_MAX;
	sstate->pend_objs = 0;

	sstate->pend_compute = 0.0;

	sstate->smoothed_ratio = 0.0;
	sstate->smoothed_int_ratio = 0.0;

	/*
	 * default setting way computation is split between the host
	 * and the storage device.
	 */
	sstate->split_type = SPLIT_DEFAULT_TYPE;
	sstate->split_ratio = SPLIT_DEFAULT_RATIO;
	;
	sstate->split_auto_step = SPLIT_DEFAULT_AUTO_STEP;
	sstate->split_bp_thresh = SPLIT_DEFAULT_BP_THRESH;
	sstate->split_mult = SPLIT_DEFAULT_MULT;
	
	sstate->exec_mode = FM_CURRENT;
	sstate->user_state = USER_UNKNOWN;

	/*
	 * Create a new thread that handles the searches for this current
	 * search.  (We probably want to make this a seperate process ??).
	 */
	err = pthread_create(&sstate->thread_id, NULL, device_main,
			     (void *) sstate);
	if (err) {
		/*
		 * XXX log 
		 */
		free(sstate);
		*app_cookie = NULL;
		exit(1);
	}

	/*
	 * thread to update the ration 
	 */
	err = pthread_create(&sstate->bypass_id, NULL, update_bypass,
			     (void *) sstate);
	if (err) {
		/*
		 * XXX log 
		 */
		free(sstate);
		*app_cookie = NULL;
	}
	
	/*
	 * Initialize our communications with the object
	 * disk sub-system.
	 */
	err = odisk_init(&sstate->ostate, NULL);
	if (err) {
		fprintf(stderr, "Failed to init the object disk \n");
		assert(0);
		return (err);
	}

	/*
	 * JIAYING: add ocache_init 
	 */
	err = ocache_init(NULL);
	if (err) {
		fprintf(stderr, "Failed to init the object cache \n");
		assert(0);
		return (err);
	}

	err = ceval_init(&sstate->cstate, sstate->ostate, (void *) sstate,
			 		sstats_drop, sstats_process);


	/*
	 * Initialize session variables storage
	 */
	session_variables_state_t *sv = malloc(sizeof(session_variables_state_t));
	pthread_mutex_init(&sv->mutex, NULL);
	sv->store = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
	sv->between_get_and_set = false;
	sstate->session_variables_state = sv;

	return (0);
}

/*
 * A request to get the characteristics of the device.  The caller must
 * free the return argument.
 */
device_char_t *
search_get_char(void *app_cookie)
{
	device_char_t   *dev_char;
	search_state_t *sstate;

	if((dev_char = (device_char_t *)malloc(sizeof(device_char_t))) == NULL) {
	  perror("malloc");
	  return NULL;
	}

	sstate = (search_state_t *) app_cookie;

	dev_char->dc_isa = DEV_ISA_IA32;
	dev_char->dc_speed = 0;
	dev_char->dc_mem = 0;

	return dev_char;
}



int
search_close_conn(void *app_cookie)
{
	/*
	 * JIAYING: may use dctl option later 
	 */
	ocache_stop(NULL);
	// exit(0);
	return (0);
}

int
search_free_obj(search_state_t * sstate, obj_data_t * obj)
{
	odisk_release_obj(obj);
	return (0);
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
		return (0);
	}

	sstate->pend_objs--;
	if (sstate->pend_objs == 0) {
		sstate->tx_idles++;
	}
	sstate->pend_compute -= obj->remain_compute;

	odisk_release_obj(obj);
	return (0);
}


/*
 * Get the current statistics on the system.  The return value must 
 * be freed by the caller.
 */

dev_stats_t *
search_get_stats(void *app_cookie)
{
	search_state_t *sstate;
	dev_stats_t    *stats;
	int             err;
	int             num_filt;
	int             len;
	float           prate;

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
		return NULL;
	}

	/*
	 * Fill in the state we can handle here.
	 */
	stats->ds_objs_total = sstate->obj_total;
	stats->ds_objs_processed = sstate->obj_processed;
	stats->ds_objs_dropped = sstate->obj_dropped;
	stats->ds_objs_nproc = sstate->obj_skipped;
	stats->ds_system_load = (int) (fexec_get_load(sstate->fdata) * 100.0);
	prate = fexec_get_prate(sstate->fdata);
	stats->ds_avg_obj_time = (long long) (prate * 1000.0);
	stats->ds_num_filters = num_filt;


	/*
	 * Get the stats for each filter.
	 */
	err =
	    fexec_get_stats(sstate->fdata, num_filt, stats->ds_filter_stats);
	if (err) {
		free(stats);
		log_message(LOGT_DISK, LOGL_ERR,
			    "search_get_stats: failed to get filter stats");
		return NULL;
	}

	return stats;
}

#define MAX_DBUF    1024
#define MAX_ENTS    512


/*
 * The caller must free the returned argument.
 */

dctl_rleaf_t *
search_read_leaf(void *app_cookie, char *path)
{
	/*
	 * XXX hack for now 
	 */
	dctl_rleaf_t      *dtype;
	int                err;
	search_state_t    *sstate;

	sstate = (search_state_t *) app_cookie;

	if((dtype = (dctl_rleaf_t *)malloc(sizeof(dctl_rleaf_t))) == NULL) {
	  perror("malloc");
	  return NULL;
	}
	if((dtype->dbuf = (char *)malloc(sizeof(char)*MAX_DBUF)) == NULL) {
	  perror("malloc");
	  return NULL;
	}

	dtype->len = MAX_DBUF;
	err = dctl_read_leaf(path, &(dtype->dt), &(dtype->len), dtype->dbuf);

	/*
	 * XXX deal with ENOSPC 
	 */

	if (err) {
	  fprintf(stderr, "dctl_read_leaf failed on: %s\n", path);
	  free(dtype);
	  return NULL;
	}

	return dtype;
}


int
search_write_leaf(void *app_cookie, char *path, int len, char *data)
{
	/*
	 * XXX hack for now 
	 */
	int             err;
	search_state_t *sstate;
	sstate = (search_state_t *) app_cookie;

	err = dctl_write_leaf(path, len, data);
	/*
	 * XXX deal with ENOSPC 
	 */

	return err;
}

dctl_lleaf_t *
search_list_leafs(void *app_cookie, char *path)
{

	/*
	 * XXX hack for now 
	 */
	dctl_lleaf_t    *lt;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	if((lt = (dctl_lleaf_t *)malloc(sizeof(dctl_lleaf_t))) == NULL) {
	  perror("malloc");
	  return NULL;
	}

	if((lt->ent_data = (dctl_entry_t *)malloc(sizeof(dctl_entry_t) * MAX_ENTS)) == NULL) {
	  perror("malloc");
	  return NULL;
	}

	lt->num_ents = MAX_ENTS;

	lt->err = dctl_list_leafs(path, &(lt->num_ents), lt->ent_data);
	/*
	 * XXX deal with ENOSPC 
	 */

	if (lt->err) {
	  free(lt->ent_data);
	  free(lt);
	  return NULL;
	}

	return lt;
}


/*
 * The caller must free the returned argument.
 */
dctl_lnode_t *
search_list_nodes(void *app_cookie, char *path)
{

	/*
	 * XXX hack for now 
	 */
	dctl_lnode_t   *lt;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;

	if((lt = (dctl_lnode_t *)malloc(sizeof(dctl_lnode_t))) == NULL) {
	  perror("malloc");
	  return NULL;
	}

	if((lt->ent_data = (dctl_entry_t *)malloc(sizeof(dctl_entry_t) * MAX_ENTS)) == NULL) {
	  perror("malloc");
	  return NULL;
	}

	lt->num_ents = MAX_ENTS;
	lt->err = dctl_list_nodes(path, &(lt->num_ents), lt->ent_data);
	/*
	 * XXX deal with ENOSPC 
	 */

	if (lt->err) {
	  free(lt->ent_data);
	  free(lt);
	  return NULL;
	}

	return lt;
}

int
search_set_gid(void *app_cookie, groupid_t gid)
{
	int             err;
	search_state_t *sstate;
	char path[PATH_MAX];
	char *prefix;
	FILE *fp;

	prefix = dconf_get_filter_cachedir();

	snprintf(path, PATH_MAX, "%s/GID_LIST", prefix);
	fp = fopen(path, "a+");
	fprintf(fp, "%llu\n", gid);
	fclose(fp);

	free(prefix);

	sstate = (search_state_t *) app_cookie;
	err = odisk_set_gid(sstate->ostate, gid);
	assert(err == 0);
	return (0);
}


int
search_clear_gids(void *app_cookie)
{
	int             err;
	search_state_t *sstate;

	/*
	 * XXX check gen num 
	 */
	sstate = (search_state_t *) app_cookie;
	err = odisk_clear_gids(sstate->ostate);
	assert(err == 0);
	return (0);
}

int
search_set_blob(void *app_cookie, char *name, int blob_len, void *blob)
{
	dev_cmd_data_t *cmd;
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

	cmd->extra_data.bdata.fname = strdup(name);
	assert(cmd->extra_data.bdata.fname != NULL);
	cmd->extra_data.bdata.blen = blob_len;
	cmd->extra_data.bdata.blob = new_blob;


	g_async_queue_push(sstate->control_ops, cmd);
	return (0);
}

int search_set_exec_mode(void *app_cookie, uint32_t mode)
{
	search_state_t *sstate;
	filter_exec_mode_t old_mode;
	
	log_message(LOGT_DISK, LOGL_TRACE, "search_set_exec_mode: %d", mode);
	
	sstate = (search_state_t *) app_cookie;
	old_mode = sstate->exec_mode;
	sstate->exec_mode = (filter_exec_mode_t) mode;
	if (old_mode != FM_MODEL && sstate->exec_mode == FM_MODEL) {
		/* enable background search */
		background_search = 1;
	} else if (old_mode == FM_MODEL && sstate->exec_mode != FM_MODEL) {
		/* stop background search */
		if (background_pid != -1) {
			kill(background_pid, SIGHUP);
		}
	}
	return(0);
}

int search_set_user_state(void *app_cookie, uint32_t state)
{
	log_message(LOGT_DISK, LOGL_TRACE, "search_set_user_state: %d", state);
	
	search_state_t *sstate;
	sstate = (search_state_t *) app_cookie;
	sstate->user_state = (user_state_t) state;
	return(0);
}


static
void session_variables_unpack(gpointer key, gpointer value, gpointer user_value)
{
  device_session_vars_t *r = (device_session_vars_t *) user_value;
  const int i = r->len;   // tricky

  r->names[i] = strdup(key);
  r->values[i] = ((session_variable_value_t *) value)->local_val;

  //printf(" unpack %d: \"%s\" -> %g\n", i, r->names[i], r->values[i]);

  r->len++;
}

device_session_vars_t *search_get_session_vars(void *app_cookie)
{
  log_message(LOGT_DISK, LOGL_TRACE, "search_get_session_vars");

  search_state_t *sstate = (search_state_t *) app_cookie;
  GHashTable *ht = sstate->session_variables_state->store;

  device_session_vars_t *result = calloc(1, sizeof(device_session_vars_t));
  if (result == NULL) {
    return NULL;
  }


  // take the session vars lock
  pthread_mutex_lock(&sstate->session_variables_state->mutex);

  // we are now between get and set, so the accumulators will
  // work a bit differently elsewhere
  sstate->session_variables_state->between_get_and_set = true;

  // alloc the result
  result->len = g_hash_table_size(ht);
  result->names = calloc(result->len, sizeof(char *));
  result->values = calloc(result->len, sizeof(double));
  if (result->names == NULL || result->values == NULL) {
    free(result->names);
    free(result->values);
    free(result);
    return NULL;
  }

  // unpack the hashtable
  // tricky: use result->len as index to foreach
  result->len = 0;
  g_hash_table_foreach(ht, session_variables_unpack, result);
  // now result->len is correct again

  // unlock
  pthread_mutex_unlock(&sstate->session_variables_state->mutex);

  return result;
}

int search_set_session_vars(void *app_cookie, device_session_vars_t *vars)
{
  log_message(LOGT_DISK, LOGL_TRACE, "search_set_session_vars");

  search_state_t *sstate = (search_state_t *) app_cookie;
  GHashTable *ht = sstate->session_variables_state->store;

  // take the session vars lock
  pthread_mutex_lock(&sstate->session_variables_state->mutex);

  // we are no longer between get and set
  sstate->session_variables_state->between_get_and_set = false;

  // update
  int len = vars->len;
  int i;
  for (i = 0; i < len; i++) {
    char *key = vars->names[i];
    session_variable_value_t *val = g_hash_table_lookup(ht, key);

    if (val == NULL) {
      // add it
      val = calloc(1, sizeof(session_variable_value_t));
      g_hash_table_replace(ht, strdup(key), val);
    }

    // set global val to new value from client
    // (this already contains our local_val)
    val->global_val = vars->values[i];

    // now reset our local_val to be the changes we accumulated while
    // the client was busy contacting other servers
    // (this is ok, because the client has fetched this value earlier,
    //  and it is now part of global_val)
    val->local_val = val->between_get_and_set_val;

    // clear the "between" accumulator
    val->between_get_and_set_val = 0.0;

    //printf(" setting %d: \"%s\" -> %g\n", i, key, val->global_val);
  }

  // unlock
  pthread_mutex_unlock(&sstate->session_variables_state->mutex);

  return 0;
}


obj_data_t *
search_reexecute_filters(void *app_cookie, const char *obj_id)
{
	search_state_t *sstate = (search_state_t *) app_cookie;
	obj_data_t *obj = NULL;
	int err;

	/* reexecute filters */
	log_message(LOGT_DISK, LOGL_TRACE, "search_reexecute_filters");
	fprintf(stderr, "reexecuting filters\n");

	/* increment the reexec_active counter will block the background
	 * filter execution thread */
	pthread_mutex_lock(&reexec_active_mutex);
	reexec_active++;
	pthread_mutex_unlock(&reexec_active_mutex);

	/* odisk_load_obj & ceval_filters2 cannot be run concurrently from
	 * different threads */
	pthread_mutex_lock(&object_eval_mutex);

	/* need a better obj_id -> obj_name mapping so that client can't just
	 * reexecute filters against any arbitrary object on the server,
	 * however the filter code that was sent by the client can already do
	 * that anyways */
	err = odisk_load_obj(sstate->ostate, &obj, obj_id);
	if (!err)  {
		//sstate->obj_reexecution_processed++;

		ceval_filters2(obj, sstate->fdata, 1, NULL,
			       sstate->exec_mode, NULL, NULL, NULL);
	}

	/* make sure to keep search state correct, pend_objs is decremented
	 * when the object is released. */
	if (obj) {
		sstate->pend_objs++;
		sstate->pend_compute += obj->remain_compute;
	}

	pthread_mutex_unlock(&object_eval_mutex);

	/* and now wake up the background thread, if necessary. */
	pthread_mutex_lock(&reexec_active_mutex);
	if (--reexec_active == 0)
	    pthread_cond_signal(&reexec_done);
	pthread_mutex_unlock(&reexec_active_mutex);

	return obj;
}


