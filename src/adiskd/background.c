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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include "sig_calc.h"
#include "ring.h"
#include "rstat.h"
#include "diamond_consts.h"
#include "diamond_types.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_sstub.h"
#include "lib_log.h"
#include "rcomb.h"
#include "lib_filterexec.h"
#include "search_state.h"
#include "dctl_common.h"
#include "lib_ocache.h"
#include "lib_dconfig.h"
#include "sys_attr.h"
#include "obj_attr.h"


#define	SAMPLE_TIME_FLOAT	0.2
#define	SAMPLE_TIME_NANO	200000000

/*
 * XXX other place 
 */
extern int      do_cleanup;
extern int      do_fork;
extern int      active_searches;

/*
 * XXX move to seperate header file !!! 
 */
#define	CONTROL_RING_SIZE	512

static int      bg_free_obj(search_state_t * sstate, obj_data_t * obj);
static int bg_shutdown = 0;

typedef struct {
	sig_val_t	filter_sig;
	unsigned int	executions;
	unsigned int	search_objects;
	unsigned int	filter_objects;
	unsigned int	drop_objects;
	unsigned int	last_run;
} filter_history_t;

#define			HISTORY_CHUNK	200
int			num_history;
int			max_history;
filter_history_t	*history_table;


typedef struct {
	char          * fname;
	sig_val_t	blob_sig;
	int             blen;
} blob_info_t;


#define	MAX_GIDS	128

groupid_t	gid_list[MAX_GIDS];
int num_gids = 0;


#define	MAX_OBJS	64
#define	MAX_BLOBS	64

typedef struct  {
	char *		fname;
	sig_val_t	spec_sig;
	int		num_objs;
	sig_val_t	objs[MAX_OBJS];
	int		num_blobs;
	blob_info_t	blob_info[MAX_BLOBS];
} filter_opts_t;

static void bg_new_search(filter_opts_t *fops);

void
load_history(  )
{
	char fname[PATH_MAX];
	char *path;
	FILE *fp;
	int cnt;

	history_table = malloc(sizeof(filter_history_t) * HISTORY_CHUNK);
	assert(history_table != NULL);
	num_history = 0;
	max_history = HISTORY_CHUNK;

	path = dconf_get_filter_cachedir();
	snprintf(fname, PATH_MAX, "%s/filter_history", path);

	fp = fopen(fname, "r");
	if (fp == NULL) {
		return;
	}

	while (1) {
		filter_history_t *fh = history_table + num_history;

		cnt = fscanf(fp, "%s %u %u %u %u %u \n", 
			fname, &fh->executions, &fh->search_objects,
			&fh->filter_objects, &fh->drop_objects, 
			&fh->last_run);
		if (cnt != 6) {
			break;
		}
		string_to_sig(fname, &fh->filter_sig);
		num_history++;

		/* XXX fix this */
		assert(num_history < max_history);
	}
	fclose(fp);
}

filter_history_t *
find_history(sig_val_t *sig)
{
	int i;
	filter_history_t *fh;

	for (i=0; i < num_history; i++) {
		fh = &history_table[i];
		if (memcmp(sig, &fh->filter_sig, sizeof(*sig)) == 0) {
			return(fh);
		}
	}

	/* we need to allocate a new entry */
	assert(num_history < max_history);	/* XXX extend */

	fh = &history_table[num_history++];
	memset(fh, 0, sizeof(*fh));

	memcpy(&fh->filter_sig, sig, sizeof(*sig));
	return(fh);
}

int
history_cmp(const void *arg1, const void *arg2)
{
	const filter_history_t *h1 = arg1;
	const filter_history_t *h2 = arg2;
	return(h2->executions - h1->executions);
}

void
sort_history()
{
	qsort(history_table, num_history, sizeof(filter_history_t),
	      history_cmp);
}


void
load_gids()
{
	char path[PATH_MAX];
	char *prefix;
	FILE *fp;
	groupid_t gid;
	int cnt, i;
	prefix = dconf_get_filter_cachedir();

	snprintf(path, PATH_MAX, "%s/GID_LIST", prefix);
	fp = fopen(path, "r");
	if (fp == NULL) {
		return;
	}

	while (1) {
		int exist = 0;
		cnt = fscanf(fp, "%llu\n", &gid);
		if (cnt != 1)
			break;

		for (i=0; i < num_gids; i++) {
			if (gid_list[i] == gid) {
				exist = 1;
				break;
			}
		}
		if ((exist == 0) && (num_gids < MAX_GIDS)) {
			gid_list[num_gids++] = gid;
		}
	}
	fclose(fp);
}


void
write_history(  )
{
	char fname[PATH_MAX];
	char *path;
	char *sigstr;
	FILE *fp;
	int i;

	path = dconf_get_filter_cachedir();
	snprintf(fname, PATH_MAX, "%s/filter_history", path);

	fp = fopen(fname, "w+");
	if (fp == NULL)
		return;

	for (i=0; i < num_history; i++) {
		filter_history_t *fh = history_table + i;

		sigstr = sig_string(&fh->filter_sig);
		fprintf(fp, "%s %u %u %u %u %u \n", 
			sigstr, fh->executions, fh->search_objects,
			fh->filter_objects, fh->drop_objects, 
			fh->last_run);
	}
	fclose(fp);
}
/*
 * This is very fragile and unsafe for loading strings.  Rewrite using
 * flex or something more appropriate. XXX TODO
 */
static filter_opts_t *
load_filter(char *path, sig_val_t *sig)
{
	filter_opts_t *fops;
	char fullname[PATH_MAX];
	char arg[PATH_MAX];
	char *sigstr;
	FILE *fp;
	int i, n;


	fops = malloc(sizeof(*fops));
	assert(fops != NULL);

	/* read in filter config file.  This is written by
	 * save_filter_state() in filter_exec.c
	 */
	sigstr = sig_string(sig);
	snprintf(fullname, PATH_MAX, FILTER_CONFIG, path, sigstr);
	fp = fopen(fullname, "r");
	free(sigstr);
	if (fp == NULL)
		return NULL;

	fscanf(fp, "FNAME %s\n", arg);
	fops->fname = strdup(arg);
	assert(fops->fname != NULL);

	n = fscanf(fp, "SPEC_SIG %s\n", arg);
	if (n != 1) {
		free(fops);
		return NULL;
	}
	string_to_sig(arg, &fops->spec_sig);


	n = fscanf(fp, "NUM_OBJECT_FILES %d\n", &fops->num_objs);
	if (n != 1) {
		free(fops);
		return NULL;
	}

	for (i = 0; i < fops->num_objs; i++) {
		fscanf(fp, "OBJECT_FILE %s\n", arg);
		if (n != 1) {
			free(fops);
			return NULL;
		}
		string_to_sig(arg, &fops->objs[i]);
	}

	n = fscanf(fp, "NUM_BLOBS %d\n", &fops->num_blobs);
	if (n != 1) {
		free(fops);
		return NULL;
	}

	for (i = 0; i < fops->num_blobs; i++) {
		fscanf(fp, "BLOBLEN %d\n", &fops->blob_info[i].blen);
		if (n != 1) {
			free(fops);
			return NULL;
		}

		fscanf(fp, "BLOBSIG %s\n", arg);
		if (n != 1) {
			free(fops);
			return NULL;
		}
		string_to_sig(arg, &fops->blob_info[i].blob_sig);

		fscanf(fp, "BLOBFILTER %s\n", arg);
		fops->blob_info[i].fname = strdup(arg);
		assert(fops->blob_info[i].fname != NULL);

	}
	return(fops);
}

static void
bg_update_cache()
{
	int i;
	struct timeval tv;
	char *path = dconf_get_filter_cachedir();
	filter_opts_t *fops;
        filter_history_t *fh;

	gettimeofday(&tv, NULL);


	for (i=0; i < num_history; i++) {
		fh = &history_table[i];
		if ((tv.tv_sec - fh->last_run) > 1800) {
			fops = load_filter(path, &fh->filter_sig);
			if (fops != NULL) {
				bg_new_search(fops);
				if (bg_shutdown == 0) {
					fh->last_run = tv.tv_sec;
					write_history();
				}
				return;
			}
		}


	}
}

static void
update_history()
{
	DIR *dir;
	char *path = dconf_get_filter_cachedir();
	char fname[PATH_MAX];
	char sig[PATH_MAX];
	unsigned int  obj, called, drop;
	struct dirent *cur_ent;
	filter_history_t *fh;
	sig_val_t sigval;
	FILE *fp;

	dir = opendir(path);

	while (1) {
		if ((cur_ent = readdir(dir)) == NULL) {
			closedir(dir);
			return;
		}

		if (cur_ent->d_type != DT_REG) {
			continue;
		}
		if (strstr(cur_ent->d_name, "results.") != cur_ent->d_name) {
			continue;
		}
		snprintf(fname, PATH_MAX, "%s/%s", path, cur_ent->d_name);
		fp = fopen(fname, "r");
		if (fp == NULL) {
			continue;
		}

		while (fscanf(fp, "%s %u %u %u \n", sig, &obj, &called, &drop) == 4) {
			string_to_sig(sig, &sigval);


			fh = find_history(&sigval);
			fh->executions++;
			fh->search_objects += obj;
			fh->filter_objects += called;
			fh->drop_objects += drop;
				
		}
		fclose(fp);
		unlink(fname);
	}

}


/*
 * Take the current command and process it.  Note, the command
 * will be free'd by the caller.
 */
static void
load_blobs(search_state_t *sstate, filter_opts_t *fops)
{
	int i, err;
	char fname[PATH_MAX];
	char *path;
	void *data;
	char *sigstr;
	FILE *fp;

	for (i=0; i < fops->num_blobs; i++) {
		path = dconf_get_blob_cachedir();
		data = malloc(fops->blob_info[i].blen);
		assert(data != NULL);

		sigstr = sig_string(&fops->blob_info[i].blob_sig);
		snprintf(fname, PATH_MAX, BLOB_FORMAT, path, sigstr);

		fp = fopen(fname, "r");
		if (fp == NULL) {
			return;
		}

		err = fread(data, fops->blob_info[i].blen, 1, fp);
		assert(err == 1);

		err = fexec_set_blob(sstate->fdata, 
			fops->blob_info[i].fname, fops->blob_info[i].blen,
			data);
		assert(err == 0);
		free(data);
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


/*
 * This is the main thread that executes a "search" on a device.
 * This interates it handles incoming messages as well as processing
 * object.
 */

static void
background_eval(void *arg)
{
	search_state_t *sstate;
	obj_data_t     *new_obj;
	int             err;
	int             pass;
	int             any;
	struct timespec timeout;
	query_info_t    qinfo;

	sstate = (search_state_t *) arg;
	log_thread_register(sstate->log_cookie);
	dctl_thread_register(sstate->dctl_cookie);
 	memset(&qinfo, 0, sizeof(query_info_t));

	while (1) {
		if (bg_shutdown) {
			break;
		}
		any = 0;

		err = odisk_next_obj(&new_obj, sstate->ostate);
		if (err == ENOENT) {
			/*
			 * We have processed all the objects,
			 * clear the running and set the complete
			 * flags.
			 */
			log_message(LOGT_DISK, LOGL_INFO, 
					"objects processed %d passed %d dropped %d", 
					sstate->obj_processed,
					sstate->obj_passed,
					sstate->obj_dropped); 
			log_message(LOGT_DISK, LOGL_INFO, 
					"bg objects processed %d passed %d dropped %d", 
					sstate->obj_bg_processed,
					sstate->obj_bg_passed,
					sstate->obj_bg_dropped); 
			return;
		} else if (err) {
			continue;
		} else {
			any = 1;
			sstate->obj_processed++;
			sstate->obj_bg_processed++;

			/* XXX force eval of desired filters */
			pass = ceval_filters2(new_obj, sstate->fdata, 1, sstate->exec_mode,
						&qinfo, sstate, continue_fn);

			if (pass == 0) {
				sstate->obj_dropped++;
				sstate->obj_bg_dropped++;
			} else {
				sstate->obj_passed++;
				sstate->obj_bg_passed++;
			}
			bg_free_obj(sstate, new_obj);
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


static void
bg_drop(void *cookie)
{
        search_state_t *sstate = (search_state_t *) cookie;
        sstate->obj_dropped++;
        sstate->obj_bg_dropped++;
}


static void
bg_process(void *cookie)
{
        search_state_t *sstate = (search_state_t *) cookie;
	sstate->obj_processed++;
	sstate->obj_bg_processed++;
}


/*
 * This is the callback that is called when a new connection
 * has been established at the network layer.  This creates
 * new search contect and creates a thread to process
 * the data. 
 */

static void
bg_new_search(filter_opts_t *fops)
{
	search_state_t *sstate;
	query_info_t qinfo;
	int err,i;

	sstate = (search_state_t *) calloc(1, sizeof(*sstate));
	if (sstate == NULL) {
		exit(1);
	}

	memset(&qinfo, 0, sizeof(query_info_t));

	dctl_init(&sstate->dctl_cookie);

	dctl_register_node(ROOT_PATH, SEARCH_NAME);

	dctl_register_leaf(DEV_SEARCH_PATH, "version_num",
			   DCTL_DT_UINT32, dctl_read_uint32, NULL,
			   &sstate->ver_no);
	dctl_register_leaf(DEV_SEARCH_PATH, "work_ahead", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32, 
			   &sstate->work_ahead);
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

	dctl_register_leaf(DEV_SEARCH_PATH, "nw_stalls", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &sstate->network_stalls);

	dctl_register_leaf(DEV_SEARCH_PATH, "tx_full_stalls", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &sstate->tx_full_stalls);

	dctl_register_leaf(DEV_SEARCH_PATH, "tx_idles", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &sstate->tx_idles);

	dctl_register_leaf(DEV_SEARCH_PATH, "pend_objs", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &sstate->pend_objs);
	dctl_register_leaf(DEV_SEARCH_PATH, "pend_maximum", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &sstate->pend_max);
	dctl_register_leaf(DEV_SEARCH_PATH, "split_type", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &sstate->split_type);
	dctl_register_leaf(DEV_SEARCH_PATH, "split_ratio", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &sstate->split_ratio);
	dctl_register_leaf(DEV_SEARCH_PATH, "split_auto_step", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &sstate->split_auto_step);
	dctl_register_leaf(DEV_SEARCH_PATH, "split_bp_thresh", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &sstate->split_bp_thresh);
	dctl_register_leaf(DEV_SEARCH_PATH, "split_multiplier",
			   DCTL_DT_UINT32, dctl_read_uint32,
			   dctl_write_uint32, &sstate->split_mult);
	dctl_register_leaf(DEV_SEARCH_PATH, "average_ratio", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL, &sstate->avg_int_ratio);
	dctl_register_leaf(DEV_SEARCH_PATH, "smoothed_beta", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL,
			   &sstate->smoothed_int_ratio);



	dctl_register_node(ROOT_PATH, DEV_NETWORK_NODE);
	dctl_register_node(ROOT_PATH, DEV_FEXEC_NODE);

	dctl_register_node(ROOT_PATH, DEV_OBJ_NODE);

	dctl_register_node(ROOT_PATH, DEV_CACHE_NODE);

	log_init(LOG_PREFIX, DEV_SEARCH_PATH, &sstate->log_cookie);
	log_message(LOGT_DISK, LOGL_DEBUG, "adiskd: new background search");

	/* initialize libfilterexec */
	fexec_system_init();

	/* init the ring to hold the queue of pending operations.  */
	err = ring_init(&sstate->control_ops, CONTROL_RING_SIZE);
	if (err) {
		free(sstate);
		exit(1);
	}

	sstate->flags = 0;

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

	err = odisk_init(&sstate->ostate, NULL, sstate->dctl_cookie,
			 sstate->log_cookie);
	if (err) {
		fprintf(stderr, "Failed to init the object disk \n");
		assert(0);
		return;
	}

	sstate->exec_mode = FM_MODEL;
	sstate->user_state = USER_UNKNOWN;

	/*
	 * JIAYING: add ocache_init 
	 */
	err = ocache_init(NULL, sstate->dctl_cookie, sstate->log_cookie);
	if (err) {
		fprintf(stderr, "Failed to init the object cache \n");
		assert(0);
		return;
	}

	err = ceval_init(&sstate->cstate, sstate->ostate, (void *) sstate,
			 bg_drop, bg_process, sstate->log_cookie);

	sstate->ver_no = 1;
	err = fexec_load_spec(&sstate->fdata, &fops->spec_sig);
	if (err) {
		return;
	}
	for (i=0; i< fops->num_objs; i++) {
		err = fexec_load_obj(sstate->fdata, &fops->objs[i]);
		if (err) {
			assert(0);
			return;
		}
	}

	/* this may not be the best approach */
	/* XXX can we do better */
	for (i=0; i < num_gids; i++) {
		fprintf(stderr, "set gidlist %llu \n", gid_list[i]);
		err = odisk_set_gid(sstate->ostate, gid_list[i]);
		assert(err == 0);
	}

	load_blobs(sstate, fops);

	/* do search start stuff */

	fexec_set_full_eval(sstate->fdata);

 	ceval_init_search(sstate->fdata, &qinfo, sstate->cstate);

	err = odisk_reset(sstate->ostate);
	if (err) {
		/* XXX */
		return;
	}
	err = ocache_start();
	if (err) {
		/* XXX */
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
	sstate->ver_no = 1;
	sstate->flags |= DEV_FLAG_RUNNING;



	/* run the eval loop */
	background_eval(sstate);

	/* cleanup from the search */
	err = odisk_flush(sstate->ostate);
	assert(err == 0);

	ceval_stop(sstate->fdata);
	ocache_stop(NULL);
	fexec_term_search(sstate->fdata);
}


int
bg_stop(void *app_cookie)
{
	bg_shutdown = 1;
	return (0);
}

int
bg_free_obj(search_state_t * sstate, obj_data_t * obj)
{
	odisk_release_obj(obj);
	return (0);
}

/*
 * This releases an object that is no longer needed.
 */

int
bg_release_obj(void *app_cookie, obj_data_t * obj)
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



int
bg_set_gid(void *app_cookie, int gen_num, groupid_t gid)
{
	int             err;
	search_state_t *sstate;

	sstate = (search_state_t *) app_cookie;
	err = odisk_set_gid(sstate->ostate, gid);
	assert(err == 0);
	return (0);
}


int
bg_clear_gids(void *app_cookie, int gen_num)
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

static void
hup_handler(int sig)
{
	bg_shutdown = 1;
}

void
start_background()
{
	signal(SIGHUP, hup_handler);
	load_gids();
	load_history();
	update_history();
	sort_history();
	write_history();
	bg_update_cache();

}


