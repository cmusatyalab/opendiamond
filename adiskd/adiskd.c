/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 5
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include <netdb.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "lib_ocache.h"
#include "sys_attr.h"
#include "lib_sstub.h"
#include "lib_filterexec.h"
#include "search_state.h"


/*
 * The default behaviors are to create a new daemon at startup time
 * and to fork whenever a new connection is established.
 */
static int             do_daemon = 1;
static int             do_fork = 1;
static int             not_silent = 0;
static int             bind_locally = 0;

static void
usage(void)
{
	fprintf(stdout, "adiskd -[abcdlhins] \n");
	fprintf(stdout, "\t -b run background tasks \n");
	fprintf(stdout, "\t -d do not run adisk as a daemon \n");
	fprintf(stdout, "\t -h get this help message \n");
	fprintf(stdout, "\t -i allow background to run when not idle\n");
	fprintf(stdout, "\t -l only listen on localhost \n");
	fprintf(stdout, "\t -n do not fork for a  new connection \n");
	fprintf(stdout, "\t -s do not close stderr on fork \n");
}


#define	SAMPLE_TIME_FLOAT	0.2
#define	SAMPLE_TIME_NANO	200000000

/* reexecution conditions */
static pthread_mutex_t	reexecute_can_start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reexecute_can_start_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t	reexecute_done_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reexecute_done_cond = PTHREAD_COND_INITIALIZER;


static int      search_free_obj(search_state_t * sstate, obj_data_t * obj);

typedef enum {
	DEV_STOP,
	DEV_TERM,
	DEV_START,
	DEV_SPEC,
	DEV_OBJ,
	DEV_BLOB,
	DEV_REEXECUTE
} dev_op_type_t;


typedef struct {
	char           *fname;
	void           *blob;
	int             blen;
} dev_blob_data_t;

typedef struct {
	bool reexecute_can_start;
	bool reexecute_done;
} dev_reexecute_data_t;

typedef struct {
	dev_op_type_t   cmd;
	sig_val_t	sig;
	union {
		dev_blob_data_t bdata;
		host_stats_t    hdata;
		unsigned int	search_id;
		dev_reexecute_data_t	reexecdata;
	} extra_data;
} dev_cmd_data_t;

static int
search_start(void *app_cookie, unsigned int search_id)
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
	cmd->extra_data.search_id = search_id;

	g_async_queue_push(sstate->control_ops, cmd);

	return (0);
}


/*
 * This is called to set the searchlet for the current search.
 */

static int
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


static int
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
	sstate->obj_processed = 0;
	sstate->obj_dropped = 0;
	sstate->obj_passed = 0;
	sstate->obj_skipped = 0;
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
		err = odisk_reset(sstate->ostate, cmd->extra_data.search_id);
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

		sstate->flags |= DEV_FLAG_RUNNING;
		break;

	case DEV_SPEC:
		/* clean up any search state from a previous run */
		if (sstate->fdata) {
		    ceval_stop(sstate->fdata);
		    fexec_term_search(sstate->fdata);
		}

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

	case DEV_REEXECUTE:
		/* signal that the device thread is quiet */
		pthread_mutex_lock(&reexecute_can_start_mutex);
		cmd->extra_data.reexecdata.reexecute_can_start = true;
		//printf("<- sending reexecute_can_start \n");
		pthread_cond_signal(&reexecute_can_start_cond);
		pthread_mutex_unlock(&reexecute_can_start_mutex);

		/* now wait for the reexecution to finish */
		pthread_mutex_lock(&reexecute_done_mutex);
		while (!cmd->extra_data.reexecdata.reexecute_done) {
			pthread_cond_wait(&reexecute_done_cond,
					  &reexecute_done_mutex);
		}
		//printf(" -> reexecute_done received\n");
		pthread_mutex_unlock(&reexecute_done_mutex);
		break;

	default:
		printf("unknown command %d \n", cmd->cmd);
		break;

	}
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
						      &qinfo);

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
						      1, &elapsed, &qinfo);
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

	return NULL;
}


/*
 * This is the callback that is called when a new connection
 * has been established at the network layer.  This creates
 * new search contect and creates a thread to process
 * the data. 
 */

static int
search_new_conn(void *comm_cookie, void **app_cookie)
{
	search_state_t *sstate;
	int             err;
	pid_t			new_proc;

	/*
	 * We have a new connection decide whether to fork or not
	 */
	if (do_fork) {
		new_proc = fork();
    } else {
		new_proc = 0;
    }
	
	if (new_proc != 0) {
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

	/*
	 * Initialize the log for the new process
	 */
	log_init(LOG_PREFIX, "cur_search");

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

static int
search_close_conn(void *app_cookie)
{
	ocache_stop(NULL);
	// exit(0);
	return (0);
}

static int
search_free_obj(search_state_t * sstate, obj_data_t * obj)
{
	odisk_release_obj(obj);
	return (0);
}

/*
 * This releases an object that is no longer needed.
 */

static int
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

static dev_stats_t *
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
	stats->ds_objs_total = odisk_get_obj_cnt(sstate->ostate);
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

static int
search_set_scope(void *app_cookie, const char *cookie)
{
	search_state_t *sstate = (search_state_t *) app_cookie;
	return odisk_set_scope(sstate->ostate, cookie);
}


static int
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

static device_session_vars_t *search_get_session_vars(void *app_cookie)
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

static int search_set_session_vars(void *app_cookie, device_session_vars_t *vars)
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


static obj_data_t *
search_reexecute_filters(void *app_cookie, const char *obj_id)
{
	/*
	 * we want to wait until the device thread is quiescent, and
	 * then have the the device thread wait until we are done
	 * with reexecution, so we will use a pair of conditions
	 */
	search_state_t *sstate = (search_state_t *) app_cookie;
	pr_obj_t *pobj;
	obj_data_t *obj = NULL;
	int err;
	dev_cmd_data_t *cmd;

	cmd = (dev_cmd_data_t *) calloc(1, sizeof(*cmd));
	if (cmd == NULL) {
		return NULL;
	}

	cmd->cmd = DEV_REEXECUTE;

	/* send message */
	g_async_queue_push(sstate->control_ops, cmd);

	/* wait for the device thread to be ready */
	pthread_mutex_lock(&reexecute_can_start_mutex);
	while (!cmd->extra_data.reexecdata.reexecute_can_start) {
		pthread_cond_wait(&reexecute_can_start_cond,
				  &reexecute_can_start_mutex);
	}
	//printf(" -> reexecute_can_start received\n");
	pthread_mutex_unlock(&reexecute_can_start_mutex);

	/* reexecute filters */
	log_message(LOGT_DISK, LOGL_TRACE, "search_reexecute_filters");
	fprintf(stderr, "reexecuting filters\n");

	/* need a better obj_id -> obj_name mapping so that client can't just
	 * reexecute filters against any arbitrary object on the server,
	 * however the filter code that was sent by the client can already do
	 * that anyways */
	pobj = ceval_filters1(strdup(obj_id), sstate->fdata, sstate->cstate);
	if (!pobj) goto done;

	err = odisk_pr_load(pobj, &obj, sstate->ostate);
	odisk_release_pr_obj(pobj);
	if (err) goto done;

	//sstate->obj_reexecution_processed++;

	ceval_filters2(obj, sstate->fdata, 1, NULL, NULL);

done:
	/* make sure to keep search state correct, pend_objs is decremented
	 * when the object is released. */
	if (obj) {
		sstate->pend_objs++;
		sstate->pend_compute += obj->remain_compute;
	}

	/* and now tell the background thread it can continue */
	pthread_mutex_lock(&reexecute_done_mutex);
	cmd->extra_data.reexecdata.reexecute_done = true;
	//printf("<- sending reexecute_done \n");
	pthread_cond_signal(&reexecute_done_cond);
	pthread_mutex_unlock(&reexecute_done_mutex);

	return obj;
}



int
main(int argc, char **argv)
{
	int             err;
	void           *cookie;
	sstub_cb_args_t cb_args;
	int             c;

	if (!g_thread_supported()) g_thread_init(NULL);

	/*
	 * Parse any of the command line arguments.
	 */

	while (1) {
		c = getopt(argc, argv, "abdhilns");
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'd':
			do_daemon = 0;
			break;

		case 'h':
			usage();
			exit(0);
			break;

		case 'l':
			bind_locally = 1;
			break;

		case 'n':
			/* this is a debugging mode where we don't
			 * want extra processes so diable most of them.
			 */
			do_fork = 0;
			do_daemon = 0;
			break;

		case 's':
			not_silent = 1;
			break;


		default:
			usage();
			exit(1);
			break;
		}
	}



	/*
	 * make this a daemon by the appropriate call 
	 */
	if (do_daemon) {
		err = daemon(1, not_silent);
		if (err != 0) {
			perror("daemon call failed !! \n");
			exit(1);
		}
	}


	cb_args.new_conn_cb = search_new_conn;
	cb_args.close_conn_cb = search_close_conn;
	cb_args.start_cb = search_start;

	cb_args.set_fspec_cb = search_set_spec;
	cb_args.set_fobj_cb = search_set_obj;

	cb_args.release_obj_cb = search_release_obj;
	cb_args.get_stats_cb = search_get_stats;
	cb_args.set_scope_cb = search_set_scope;
	cb_args.set_blob_cb = search_set_blob;
	cb_args.get_session_vars_cb = search_get_session_vars;
	cb_args.set_session_vars_cb = search_set_session_vars;
	cb_args.reexecute_filters = search_reexecute_filters;

	cookie = sstub_init(&cb_args, bind_locally);
	if (cookie == NULL) {
		fprintf(stderr, "Unable to initialize the communications\n");
		exit(1);
	}


	/*
	 * We provide the main thread for the listener.  This
	 * should never return.
	 */
	while (1) {
		sstub_listen(cookie);

		/*
		 * We need to do a periodic wait. To get rid
		 * of all the zombies.  The posix spec appears to
		 * be fuzzy on the behavior of setting sigchild
		 * to ignore, so we will do a periodic nonblocking
		 * wait to clean up the zombies.
		 */
		waitpid(-1, NULL, WNOHANG | WUNTRACED);
	}

	exit(0);
}
