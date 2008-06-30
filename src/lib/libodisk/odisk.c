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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_dconfig.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "lib_ocache.h"
#include "dctl_common.h"
#include "odisk_priv.h"
#include "sys_attr.h"
#include "dconfig_priv.h"
#include "ocache_priv.h"
#include "rtimer.h"
#include "tools_priv.h"
#include "sig_calc_priv.h"
#include "ring.h"


#define	MAX_READ_THREADS	1

#define	CACHE_EXT	".CACHEFL"

/*
 * XXX shared state , move into state descriptor ???
 */
static int      search_active = 0;
static int      search_done = 0;
static ring_data_t *obj_ring;
static pthread_mutex_t odisk_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fg_data_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t bg_active_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t bg_queue_cv = PTHREAD_COND_INITIALIZER;

#define	OBJ_RING_SIZE	32

static ring_data_t *obj_pr_ring;
static pthread_cond_t pr_fg_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t pr_bg_queue_cv = PTHREAD_COND_INITIALIZER;
#define OBJ_PR_RING_SIZE        32
/*
 * These are the set of group ID's we are using to 
 * filter the data.
 */
#define MAX_GID_FILTER  64

static int
odisk_next_index_ent(FILE * idx_file, char *file_name)
{
	int             offset;
	int             val;

	offset = 0;
	while (offset < (NAME_MAX - 1)) {
		val = fgetc(idx_file);
		if (val == EOF) {
			if (offset > 0) {
				file_name[offset++] = '\0';
				return (1);
			} else {
				return (0);
			}
		}
		if ((val == '\0') || (val == '\n')) {
			if (offset > 0) {
				file_name[offset++] = '\0';
				return (1);
			}
		} else {
			file_name[offset++] = (char) val;
		}
	}

	/*
	 * if we get here we have overflowed ... 
	 */
	file_name[offset++] = '\0';
	return (1);
}

/*
 * Set an attribute if it is not defined to the name value passed in 
 */
static void
obj_set_notdef(obj_data_t * obj, char *attr_name, void *val, size_t len)
{
	int             err;
	size_t          rlen;

	rlen = 0;

	err = obj_read_attr(&obj->attr_info, attr_name, &rlen, NULL);
	if (err == ENOENT) {
		err = obj_write_attr(&obj->attr_info, attr_name, len, val);
	}

}

/*
 * Set some system defined attributes if they are not already defined on
 * the object.
 */
static void
obj_set_sysattr(odisk_state_t * odisk, obj_data_t * obj, char *name)
{
	size_t          len;

	/*
	 * set the object name 
	 */
	len = strlen(name) + 1;
	obj_set_notdef(obj, DISPLAY_NAME, name, len);

	/*
	 * set the object name 
	 */
	len = strlen(odisk->odisk_name) + 1;
	obj_set_notdef(obj, DEVICE_NAME, odisk->odisk_name, len);


}

int
odisk_load_obj(odisk_state_t * odisk, obj_data_t ** obj_handle, char *name)
{
	obj_data_t     *new_obj;
	struct stat     stats;
	int             os_file;
	char           *data;
	char           *base;
	int             err,
	                len;
	size_t          size;
	uint64_t        local_id;
	char           *ptr;
	char            attr_name[NAME_MAX];

	assert(name != NULL);
	if (strlen(name) >= NAME_MAX) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_load_obj: file name <%s> exceeds NAME_MAX", name);
		return (EINVAL);
	}

	new_obj = malloc(sizeof(*new_obj));
	assert(new_obj != NULL);

	/*
	 * open and read the data file
	 */
	os_file = open(name, odisk->open_flags);
	if (os_file == -1) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_load_obj: open file <%s> failed", name);
		free(new_obj);
		return (ENOENT);
	}

	err = fstat(os_file, &stats);
	if (err != 0) {
		free(new_obj);
		return (ENOENT);
	}

	base = (char *) malloc(stats.st_size);
	assert(base != NULL);
	data = (char *) base;
	if (base == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
		     "odisk_load_obj: failed to allocate storage for <%s>",
		     name);
		close(os_file);
		free(new_obj);
		return (ENOENT);
	}

	if (stats.st_size > 0) {
		size = read(os_file, data, stats.st_size);
		if (size != stats.st_size) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "odisk_load_obj: load file <%s> failed", name);
			free(base);
			close(os_file);
			free(new_obj);
			return (ENOENT);
		}
	}
	close(os_file);
	new_obj->data = data;
	new_obj->data_len = stats.st_size;
	new_obj->ref_count = 1;
	sig_cal_str(name, &new_obj->id_sig);
	pthread_mutex_init(&new_obj->mutex, NULL);

	ptr = rindex(name, '/');
	if (ptr == NULL) {
		ptr = name;
	} else {
		ptr++;
	}
	sscanf(ptr, "OBJ%016llX", &local_id);
	new_obj->local_id = local_id;

	/*
	 * Load the binary attributes, if any.
	 */
	len = snprintf(attr_name, NAME_MAX, "%s%s", name, BIN_ATTR_EXT);
	assert(len < NAME_MAX);
	obj_read_attr_file(odisk, attr_name, &new_obj->attr_info);

	/*
	 * Load text attributes, if any.
	 */
	len = snprintf(attr_name, NAME_MAX, "%s%s", name, TEXT_ATTR_EXT);
	assert(len < NAME_MAX);
	obj_load_text_attr(odisk, attr_name, new_obj);


	/*
	 * set any system specific attributes for the object 
	 */
	obj_set_sysattr(odisk, new_obj, name);

	*obj_handle = (obj_data_t *) new_obj;
	odisk->obj_load++;

	/* add attributes to the ocache */
	ocache_add_initial_attrs(*obj_handle);

	return (0);
}



float
odisk_get_erate(odisk_state_t * odisk)
{
	return (ring_erate(obj_ring));
}

int
odisk_get_obj_cnt(odisk_state_t * odisk)
{
	int             count = 0;
	char            idx_file[NAME_MAX];
	char            file_name[NAME_MAX];
	FILE           *new_file;
	int             i;
	int             len;
	int             ret;

	for (i = 0; i < odisk->num_gids; i++) {
		len = snprintf(idx_file, NAME_MAX, "%s/%s%016llX",
			       odisk->odisk_indexdir, GID_IDX,
			       odisk->gid_list[i]);
		assert(len < NAME_MAX);
		new_file = fopen(idx_file, "r");
		if (new_file == NULL) {
			continue;
		}
		while ((ret = odisk_next_index_ent(new_file, file_name))) {
			count++;
		}
		fclose(new_file);
	}
	return (count);
}

int
odisk_get_obj(odisk_state_t * odisk, obj_data_t ** obj, obj_id_t * oid)
{
	char            buf[NAME_MAX];
	int             err;
	int             len;


	len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_dataroot,
		       oid->local_id);

	assert(len < NAME_MAX);

	err = odisk_load_obj(odisk, obj, buf);
	if (err == 0) {
		(*obj)->local_id = oid->local_id;
	} else {
		printf("get obj failed \n");
	}
	return (err);
}

void
odisk_ref_obj(obj_data_t * obj)
{

	/*
	 * increment ref count 
	 */
	pthread_mutex_lock(&obj->mutex);
	obj->ref_count++;
	pthread_mutex_unlock(&obj->mutex);

	return;
}

int
odisk_release_obj(obj_data_t * obj)
{
	obj_adata_t    *cur, *next;

	assert(obj != NULL);

	/*
	 * decrement ref count 
	 */
	pthread_mutex_lock(&obj->mutex);
	obj->ref_count--;
	if (obj->ref_count != 0) {
		pthread_mutex_unlock(&obj->mutex);
		return (0);
	}

	/*
	 * we can release the lock now because we own the last reference
	 * (or else someone screwed up).
	 */
	pthread_mutex_unlock(&obj->mutex);

	if (obj->data != NULL) {
		free(obj->data);
	}

	cur = obj->attr_info.attr_dlist;
	while (cur != NULL) {
		next = cur->adata_next;
		free(cur->adata_data);
		free(cur);
		cur = next;
	}

	pthread_mutex_destroy(&obj->mutex);
	free(obj);
	return (1);
}

static void
odisk_release_pr_obj(pr_obj_t * pobj)
{
	if (pobj == NULL) {
		return;
	}

	if (pobj->obj_name != NULL) {
		free(pobj->obj_name);
	}

	free(pobj);
	return;
}

int
odisk_clear_gids(odisk_state_t * odisk)
{
	odisk->num_gids = 0;
	return (0);
}

int
odisk_set_gid(odisk_state_t * odisk, groupid_t gid)
{
	int             i;

	/*
	 * make sure this GID is not already in the list 
	 */
	for (i = 0; i < odisk->num_gids; i++) {
		if (odisk->gid_list[i] == gid) {
			return (0);
		}
	}

	/*
	 * make sure there is room for this new entry.
	 */
	if (odisk->num_gids >= MAX_GID_FILTER) {
		return (ENOMEM);
	}

	odisk->gid_list[odisk->num_gids] = gid;
	odisk->num_gids++;
	return (0);
}

static int
odisk_pr_next(pr_obj_t ** new_object)
{
	pr_obj_t       *tmp;

	pthread_mutex_lock(&odisk_mutex);
	while (1) {
		if (!ring_empty(obj_pr_ring)) {
			tmp = ring_deq(obj_pr_ring);

			pthread_cond_signal(&pr_bg_queue_cv);
			if (tmp->oattr_fnum == -1) {
				free(tmp);
				search_done = 1;
			} else {
				*new_object = tmp;
				pthread_mutex_unlock(&odisk_mutex);
				return (0);
			}
		} else {
			if (search_done) {
				*new_object = NULL;
				pthread_mutex_unlock(&odisk_mutex);
				return (ENOENT);
			}
			pthread_cond_wait(&pr_fg_cv, &odisk_mutex);
		}
	}
}

static int
odisk_pr_load(pr_obj_t * pr_obj, obj_data_t ** new_object,
	      odisk_state_t * odisk)
{
	int             err;
	int		i;
	char		timebuf[BUFSIZ];
	rtimer_t	rt;
	u_int64_t	time_ns;
	u_int64_t	stack_ns;

	assert(pr_obj != NULL);
	stack_ns = pr_obj->stack_ns;

	/*
	 * Load base object 
	 */

	err = odisk_load_obj(odisk, new_object, pr_obj->obj_name);
	if (err) {
		printf("load obj <%s> failed %d \n", pr_obj->obj_name, err);
		return (err);
	}

	/*
	 * see if we had ocache hits, in which case we may have cached
	 * attributes to load 
	 */
	if (pr_obj->oattr_fnum == 0)
		return (0);

	for (i = 0; i < pr_obj->oattr_fnum; i++) {
		if (pr_obj->filters[i] == NULL)
			continue;

		rt_init(&rt);
		rt_start(&rt);

		/* get cached attribute values from the ocache.db if possible */
		err = cache_read_oattrs(&(*new_object)->attr_info,
					pr_obj->filter_hits[i]);

		rt_stop(&rt);
		time_ns = rt_nanos(&rt);
		stack_ns += time_ns;

		/* cache_read_oattrs returns with an error if we failed to
		 * add the attributes to new_object or if there was no
		 * cached attribute for this filter. continue to the next hit */
		if (err != 0)
			continue;

		/* update total filter run time */
		sprintf(timebuf, FLTRTIME_FN, pr_obj->filters[i]);
		err = obj_write_attr(&(*new_object)->attr_info, timebuf,
				     sizeof(time_ns), (void *) &time_ns);
		if (err != 0)
			printf("CHECK OBJECT %016llX ATTR FILE\n",
			       pr_obj->obj_id);
	}

	/* update total filter run time */
	err = obj_write_attr(&(*new_object)->attr_info,
			     FLTRTIME, sizeof(stack_ns), (void *) &stack_ns);
	if (err != 0)
		printf("CHECK OBJECT %016llX ATTR FILE\n", pr_obj->obj_id);
	return (0);
}

int
odisk_pr_add(pr_obj_t * pr_obj)
{
	pthread_mutex_lock(&odisk_mutex);

	/*
	 * Loop until there is space on the queue to put the object
	 * or we find out the search has gone inactive.
	 */
	while (1) {
		if (search_active == 0) {
			odisk_release_pr_obj(pr_obj);
			pthread_mutex_unlock(&odisk_mutex);
			return (0);
		}

		if (!ring_full(obj_pr_ring)) {
			ring_enq(obj_pr_ring, pr_obj);
			pthread_cond_signal(&pr_fg_cv);
			pthread_mutex_unlock(&odisk_mutex);
			return (0);
		} else {
			pthread_cond_wait(&pr_bg_queue_cv, &odisk_mutex);
		}
	}
}


char           *
odisk_next_obj_name(odisk_state_t * odisk)
{
	char            file_name[NAME_MAX];
	char            path_name[NAME_MAX];
	char           *ncopy;
	int             i, ret;

again:
	for (i = odisk->cur_file; i < odisk->max_files; i++) {
		if (odisk->index_files[i] != NULL) {
			ret = odisk_next_index_ent(odisk->index_files[i],
						   file_name);
			if (ret == 1) {
				ret = snprintf(path_name, NAME_MAX, "%s/%s",
					odisk->odisk_dataroot, file_name);
				if (ret >= NAME_MAX) {
					log_message(LOGT_DISK, LOGL_ERR,
			    			"next_obj_name: name to big");
					continue;
				}
				ncopy = strdup(path_name);
				odisk->cur_file = i;
				return (ncopy);
			} else {
				fclose(odisk->index_files[i]);
				odisk->index_files[i] = NULL;
			}
		}
	}

	/*
	 * if we get here, either we need to start at the begining,
	 * or there is no more data.
	 */
	if (odisk->cur_file != 0) {
		odisk->cur_file = 0;
		goto again;
	} else {
		return (NULL);
	}
}


int
odisk_flush(odisk_state_t * odisk)
{
	pr_obj_t       *pobj;
	obj_data_t     *obj;
	int             err;

	err = pthread_mutex_lock(&odisk_mutex);
	assert(err == 0);
	search_active = 0;

	/*
	 * drain the pr ring 
	 */
	while (!ring_empty(obj_pr_ring)) {
		pobj = ring_deq(obj_pr_ring);
		if (pobj != NULL) {
			odisk_release_pr_obj(pobj);
		}
	}

	/*
	 * drain the object ring 
	 */
	while (!ring_empty(obj_ring)) {
		obj = ring_deq(obj_ring);
		if (obj != NULL) {
			odisk_release_obj(obj);
		}
	}

	pthread_cond_signal(&pr_bg_queue_cv);
	/*
	 * wake up all threads since we are shutting down 
	 */
	pthread_cond_broadcast(&bg_queue_cv);

	err = pthread_mutex_unlock(&odisk_mutex);
	assert(err == 0);
	printf("odisk_flush done\n");

	return (0);
}

static void    *
odisk_main(void *arg)
{
	odisk_state_t  *ostate = (odisk_state_t *) arg;
	pr_obj_t       *pobj;
	obj_data_t     *nobj = NULL;
	int             err;

	while (1) {
		/* If there is no search is active we hang out for a while */
		while (search_active == 0) {
			usleep(10000);
		}
		/*
		 * get the next object. this is a blocking call
		 * so we are guaranteed to get an object if there are
		 * any left.
		 */
		err = odisk_pr_next(&pobj);
		if (err == ENOENT) {
			search_active = 0;
			search_done = 1;
			pthread_mutex_lock(&odisk_mutex);
			pthread_cond_signal(&fg_data_cv);
			pthread_mutex_unlock(&odisk_mutex);
			continue;
		} else if (err) {
			odisk_release_pr_obj(pobj);
			continue;
		}

		err = odisk_pr_load(pobj, &nobj, ostate);
		odisk_release_pr_obj(pobj);

		if (err) {
			log_message(LOGT_DISK, LOGL_ERR,
			    "odisk_main: failed to load object");
		}

		/*
		 * We have an object put it into the queue to process.
		 * The queue may be full, so we will block on a condition
		 * variable to make sure we don't drop it.
		 */
		pthread_mutex_lock(&odisk_mutex);
		while (1) {
			if (search_active == 0) {
				odisk_release_obj(nobj);
				break;
			}

			/*
			 * try to enqueue the object, if the ring is full we
			 * will get an error.  If error we sleep until more
			 * space is available.
			 */
			err = ring_enq(obj_ring, nobj);
			if (err == 0) {
				pthread_cond_signal(&fg_data_cv);
				break;
			} else {
				pthread_cond_wait(&bg_queue_cv, &odisk_mutex);
			}
		}
		pthread_mutex_unlock(&odisk_mutex);
	}
}

int
odisk_next_obj(obj_data_t ** new_object, odisk_state_t * odisk)
{

	pthread_mutex_lock(&odisk_mutex);
	while (1) {
		if (!ring_empty(obj_ring)) {
			*new_object = ring_deq(obj_ring);
			pthread_cond_signal(&bg_queue_cv);
			pthread_mutex_unlock(&odisk_mutex);
			return (0);
		} else {
			if (search_done) {
				pthread_mutex_unlock(&odisk_mutex);
				return (ENOENT);
			}
			odisk->next_blocked++;
			pthread_cond_wait(&fg_data_cv, &odisk_mutex);
		}
	}
}


int
odisk_num_waiting(odisk_state_t * odisk)
{
	return (ring_count(obj_ring));
}



int
odisk_init(odisk_state_t ** odisk, char *dirp)
{
	odisk_state_t  *new_state;
	int             err;
	char           *dataroot;
	char           *indexdir;
	int             i;

	if (dirp == NULL) {
		dataroot = dconf_get_dataroot();
	} else {
		dataroot = strdup(dirp);
	}

	if (strlen(dataroot) > (MAX_DIR_PATH - 1)) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_init: dataroot (%s) exceeds MAX_DIR_PATH",
		    dataroot);
		free(dataroot);
		return (EINVAL);
	}

	indexdir = dconf_get_indexdir();
	if (strlen(indexdir) > (MAX_DIR_PATH - 1)) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_init: indexdir (%s) exceeds MAX_DIR_PATH",
		    indexdir);
		free(dataroot);
		free(indexdir);
		return (EINVAL);
	}

	/*
	 * make sure we have a reasonable umask 
	 */
	umask(0);

	/*
	 * clear umask so we get file permissions we specify 
	 */
	ring_init(&obj_ring, OBJ_RING_SIZE);
	ring_init(&obj_pr_ring, OBJ_PR_RING_SIZE);

	new_state = (odisk_state_t *) calloc(1, sizeof(*new_state));
	assert(new_state != NULL);

	dctl_register_leaf(DEV_OBJ_PATH, "obj_load", DCTL_DT_UINT32,
			   dctl_read_uint32, NULL,
			   &new_state->obj_load);
	dctl_register_leaf(DEV_OBJ_PATH, "next_blocked",
			   DCTL_DT_UINT32, dctl_read_uint32, NULL,
			   &new_state->next_blocked);
	dctl_register_leaf(DEV_OBJ_PATH, "readahead_blocked",
			   DCTL_DT_UINT32, dctl_read_uint32, NULL,
			   &new_state->readahead_full);

	/*
	 * the length has already been tested above 
	 */
	strcpy(new_state->odisk_dataroot, dataroot);
	strcpy(new_state->odisk_indexdir, indexdir);

	free(dataroot);
	free(indexdir);

	/*
	 * get the host name 
	 */
	err = gethostname(new_state->odisk_name, MAX_HOST_NAME);
	if (err) {
		sprintf(new_state->odisk_name, "Unknown");
	}
	new_state->odisk_name[MAX_HOST_NAME - 1] = '0';
	new_state->open_flags = O_RDONLY;

	for (i = 0; i < MAX_READ_THREADS; i++) {
		err = pthread_create(&new_state->thread_id, NULL,
				     odisk_main, (void *) new_state);
	}

	*odisk = new_state;
	return (0);
}


int
odisk_reset(odisk_state_t * odisk)
{
	char            idx_file[NAME_MAX];
	FILE           *new_file;
	int             i;
	int             len;

	/*
	 * First go through all the index files and close them.
	 */
	for (i = 0; i < odisk->max_files; i++) {
		if (odisk->index_files[i] != NULL) {
			fclose(odisk->index_files[i]);
			odisk->index_files[i] = NULL;
		}
	}

	for (i = 0; i < odisk->num_gids; i++) {
		len = snprintf(idx_file, NAME_MAX, "%s/%s%016llX",
			       odisk->odisk_indexdir, GID_IDX,
			       odisk->gid_list[i]);
		assert(len < NAME_MAX);
		new_file = fopen(idx_file, "r");
		if (new_file == NULL) {
			fprintf(stderr, "unable to open idx %s \n", idx_file);
		} else {
			odisk->index_files[i] = new_file;
		}
	}

	odisk->max_files = odisk->num_gids;
	odisk->cur_file = 0;

	pthread_mutex_lock(&odisk_mutex);
	search_active = 1;
	search_done = 0;
	pthread_cond_signal(&bg_active_cv);
	pthread_mutex_unlock(&odisk_mutex);

	return (0);
}


int
odisk_continue(void)
{
	pthread_mutex_lock(&odisk_mutex);
	search_active = 1;
	search_done = 0;
	pthread_cond_signal(&bg_active_cv);
	pthread_mutex_unlock(&odisk_mutex);
	return(0);
}

int
odisk_term(odisk_state_t * odisk)
{
	int             err;


	err = closedir(odisk->odisk_dir);

	odisk->odisk_dir = NULL;

	free(odisk);
	return (err);
}

/*
 * Create "special" null object that indicates the end of a data
 * stream.  This is a bit of a hack, but ...
 */

obj_data_t     *
odisk_null_obj(void)
{
	obj_data_t     *new_obj;

	new_obj = (obj_data_t *) malloc(sizeof(*new_obj));
	assert(new_obj != NULL);

	new_obj->data_len = 0;
	new_obj->data = NULL;
	new_obj->ref_count = 1;
	pthread_mutex_init(&new_obj->mutex, NULL);

	new_obj->attr_info.attr_ndata = 0;
	new_obj->attr_info.attr_dlist = NULL;

	return (new_obj);
}
