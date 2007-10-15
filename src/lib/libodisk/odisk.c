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
#include "dctl_common.h"
#include "odisk_priv.h"
#include "sys_attr.h"


#define	MAX_READ_THREADS	1

#define	CACHE_EXT	".CACHEFL"

static unsigned int    dynamic_load = 1;
static unsigned int    dynamic_load_depth = 3;

/*
 * forward declarations 
 */
static void     update_gid_idx(odisk_state_t * odisk, char *name,
			       groupid_t * gid);
static void     delete_object_gids(odisk_state_t * odisk, obj_data_t * obj);

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

int
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
 * Decide if we should load the output attributes
 * from the disk. This uses a simple threshold to see
 * if we are I/O bound.  If we have less than N objects then
 * we decide we are behind and skip (return 0) else
 * we return (1).
 */

static int
dynamic_load_oattr(int ring_depth)
{
	if (dynamic_load == 0) {
		return (1);
	}

	if (ring_depth < dynamic_load_depth) {
		return (0);
	} else {
		return (1);
	}

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
	 * open and read the data file, since we are streaming through * the
	 * data we try to use the direct reads to save the memcopy * in the
	 * buffer cache. 
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

	base = (char *) malloc(ALIGN_SIZE(stats.st_size));
	assert(base != NULL);
	data = (char *) ALIGN_VAL(base);
	if (base == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
		     "odisk_load_obj: failed to allocate storage for <%s>",
		     name);
		close(os_file);
		free(new_obj);
		return (ENOENT);
	}

	if (stats.st_size > 0) {
		size = read(os_file, data, ALIGN_ROUND(stats.st_size));
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
	new_obj->base = base;
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
odisk_save_obj(odisk_state_t * odisk, obj_data_t * obj)
{
	char            buf[NAME_MAX];
	char            attrbuf[NAME_MAX];
	FILE           *os_file;
	int             size;
	int             len;

	len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_dataroot,
		       obj->local_id);
	if (len >= NAME_MAX) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_save_obj: name <%s/OBJ%016llX> exceeds NAME_MAX", 
		    odisk->odisk_dataroot, obj->local_id);
		return(EINVAL);
	}


	/*
	 * open the file 
	 */
	os_file = fopen(buf, "wb");
	if (os_file == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_save_obj: unable to open file <%s>", buf);
		return (ENOENT);
	}

	if (obj->data_len > 0) {
		size = fwrite(obj->data, obj->data_len, 1, os_file);
		if (size != 1) {
			log_message(LOGT_DISK, LOGL_ERR,
			     "odisk_save_obj: failed write to file <%s>", buf);
			fclose(os_file);
			unlink(buf);
			return (ENOENT);
		}
	}

	/*
	 * save the attributes associated with the file 
	 */
	len = snprintf(attrbuf, NAME_MAX, "%s%s", buf, BIN_ATTR_EXT);
	assert(len < NAME_MAX);
	obj_write_attr_file(attrbuf, &obj->attr_info);


	fclose(os_file);
	return (0);
}

int
odisk_delete_obj(odisk_state_t * odisk, obj_data_t * obj)
{
	char            buf[NAME_MAX];
	int             len;
	int             err;

	delete_object_gids(odisk, obj);

	len = snprintf(buf, NAME_MAX, "%s/OBJ%016llX", odisk->odisk_dataroot,
		       obj->local_id);
	assert(len < NAME_MAX);
	err = unlink(buf);
	if (err == -1) {
		fprintf(stderr, "failed unlink %s:", buf);
		perror("");
	}

	/*
	 * find the attributes and destroy if they exist 
	 */
	len =
	    snprintf(buf, NAME_MAX, "%s/OBJ%016llX%s", odisk->odisk_dataroot,
		     obj->local_id, BIN_ATTR_EXT);
	assert(len < NAME_MAX);

	err = unlink(buf);
	if (err == -1) {
		fprintf(stderr, "failed unlink %s:", buf);
		perror("");
	}

	return (0);
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

	if (obj->base != NULL) {
		free(obj->base);
	}

	cur = obj->attr_info.attr_dlist;
	while (cur != NULL) {
		next = cur->adata_next;
		free(cur->adata_base);
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
odisk_add_gid(odisk_state_t * odisk, obj_data_t * obj, groupid_t * gid)
{
	gid_list_t     *glist;
	size_t          len;
	int             i,
	                err;
	int             space;

	len = 0;
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
	if (err == ENOENT) {
		glist = (gid_list_t *) calloc(1, GIDLIST_SIZE(4));
		assert(glist != NULL);
	} else if (err != ENOMEM) {
		return (err);
	} else {
		glist = (gid_list_t *) malloc(len);
		assert(glist != NULL);
		err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len,
				  (unsigned char *) glist);
		assert(err == 0);
	}

	space = -1;
	for (i = 0; i < glist->num_gids; i++) {
		if ((glist->gids[i] == 0) && (space == -1)) {
			space = i;
		}
		if (glist->gids[i] == *gid) {
			return (EAGAIN);
		}
	}

	if (space == -1) {
		int             old, new;
		old = glist->num_gids;
		new = old + 4;
		glist = realloc(glist, GIDLIST_SIZE(new));
		assert(glist != NULL);

		for (i = old; i < new; i++) {
			glist->gids[i] = 0;
		}
		glist->num_gids = new;
		space = old;
	}

	glist->gids[space] = *gid;
	err = obj_write_attr(&obj->attr_info, GIDLIST_NAME,
		     GIDLIST_SIZE(glist->num_gids), (unsigned char *) glist);
	assert(err == 0);

	return (0);
}


int
odisk_rem_gid(odisk_state_t * odisk, obj_data_t * obj, groupid_t * gid)
{
	gid_list_t     *glist;
	size_t          len;
	int             i, err;

	len = 0;
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
	if (err != ENOMEM) {
		return (err);
	}

	glist = (gid_list_t *) malloc(len);
	assert(glist != NULL);
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME,
			    &len, (unsigned char *) glist);
	assert(err == 0);

	for (i = 0; i < glist->num_gids; i++) {
		if (glist->gids[i] == *gid) {
			glist->gids[i] = *gid;
			err = obj_write_attr(&obj->attr_info, GIDLIST_NAME,
				     GIDLIST_SIZE(glist->num_gids),
				     (unsigned char *) glist);
			return (0);
		}
	}

	return (ENOENT);
}

int
odisk_new_obj(odisk_state_t * odisk, obj_id_t * oid, groupid_t * gid)
{
	char            buf[NAME_MAX];
	uint64_t        local_id;
	int             fd;
	obj_data_t     *obj;
	int             len;

	local_id = rand();

	while (1) {
		len =
		    snprintf(buf, NAME_MAX, "%s/OBJ%016llX",
			     odisk->odisk_dataroot, local_id);
		assert(len < NAME_MAX);
		fd = open(buf, O_CREAT | O_EXCL, 0777);
		if (fd == -1) {
			local_id = rand();
		} else {
			break;
		}
	}

	oid->local_id = local_id;
	close(fd);

	odisk_get_obj(odisk, &obj, oid);
	odisk_add_gid(odisk, obj, gid);

	len = snprintf(buf, NAME_MAX, "OBJ%016llX", local_id);
	assert(len < NAME_MAX);

	update_gid_idx(odisk, buf, gid);

	odisk_save_obj(odisk, obj);
	odisk_release_obj(obj);

	return (0);
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

int
odisk_write_obj(odisk_state_t * odisk, obj_data_t * obj, int len,
		int offset, char *data)
{
	int             total_len;
	char           *dbuf;

	total_len = offset + len;

	if (total_len > obj->data_len) {
		dbuf = (char *) malloc(total_len);
		assert(dbuf != NULL);
		memcpy(dbuf, obj->data, obj->data_len);
		obj->data_len = total_len;
		free(obj->base);
		obj->data = dbuf;
		obj->base = dbuf;
	}

	memcpy(&obj->data[offset], data, len);

	return (0);
}


int
odisk_read_obj(odisk_state_t * odisk, obj_data_t * obj, int *len,
	       int offset, char *data)
{
	int             rlen;
	int             remain;

	if (offset >= obj->data_len) {
		*len = 0;
		return (0);
	}

	remain = obj->data_len - offset;
	if (remain > *len) {
		rlen = *len;
	} else {
		rlen = remain;
	}


	memcpy(data, &obj->data[offset], rlen);

	*len = rlen;

	return (0);
}


int
odisk_read_next(obj_data_t ** new_object, odisk_state_t * odisk)
{
	char            path_name[NAME_MAX];
	char            file_name[NAME_MAX];
	int             err;
	int             i;
	int             ret;
	int             len;


      again:
	for (i = odisk->cur_file; i < odisk->max_files; i++) {
		if (odisk->index_files[i] != NULL) {
			ret = odisk_next_index_ent(odisk->index_files[i],
						   file_name);
			if (ret == 0) {
				/*
				 * This file done, close it and continue.  
				 */
				fclose(odisk->index_files[i]);
				odisk->index_files[i] = NULL;
				continue;
			}
			len = snprintf(path_name, NAME_MAX, "%s/%s",
				       odisk->odisk_dataroot, file_name);
			assert(len < NAME_MAX);

			err = odisk_load_obj(odisk, new_object, path_name);


			/*
			 * if we can't load the object it probably go 
			 * deleted between the time the search started 
			 * (and we got the gidindex file and the time 
			 * we tried to open it.  We just continue on. 
			 */
			if (err) {
				continue;
			}
			odisk->cur_file = i + 1;
			if (odisk->cur_file >= odisk->max_files) {
				odisk->cur_file = 0;
			}
			return (0);
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
		return (ENOENT);
	}
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
	int             i;
	char            timebuf[BUFSIZ];
	rtimer_t        rt;
	u_int64_t       time_ns;
	u_int64_t       stack_ns;

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
	 * see if we have partials to load 
	 */
	if ((pr_obj->oattr_fnum == 0) ||
	    (dynamic_load_oattr(ring_count(obj_ring)) == 0)) {
		return (0);
	}

	/*
	 * load the partial state 
	 */
	for (i = 0; i < pr_obj->oattr_fnum; i++) {

		if (pr_obj->filters[i] == NULL) {
			continue;
		}

		rt_init(&rt);
		rt_start(&rt);

		err = obj_read_oattr(odisk, odisk->odisk_dataroot,
		    &(*new_object)->id_sig, &pr_obj->fsig[i],
		    &pr_obj->iattrsig[i], &(*new_object)->attr_info);

		rt_stop(&rt);
		time_ns = rt_nanos(&rt);
		stack_ns += time_ns;

		/* if attribute read failed, we exit */
		if (err != 0) {
			break;
		}

		sprintf(timebuf, FLTRTIME_FN, pr_obj->filters[i]);
		err = obj_write_attr(&(*new_object)->attr_info, timebuf,
				     sizeof(time_ns), (void *) &time_ns);
		if (err != 0) {
			/* XXX */
			printf("CHECK OBJECT %016llX ATTR FILE\n",
			       pr_obj->obj_id);
		}
	}

	err = obj_write_attr(&(*new_object)->attr_info,
			     FLTRTIME, sizeof(stack_ns), (void *) &stack_ns);
	if (err != 0) {
		printf("CHECK OBJECT %016llX ATTR FILE\n", pr_obj->obj_id);
	}
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



#define MAX_TEMP_NAME   64
#define TEST_BUF_SIZE   8192

/*
 * we want to test the open flags, there has been some problems
 * using  O_DIRECT on some platforms.
 */
void
odisk_setup_open_flags(odisk_state_t * odisk)
{
	char           *test_name;
	char           *buf;
	int             test_fd;
	ssize_t         wsize;
	char           *base;
	char           *data;

	test_name = (char *) malloc(MAX_TEMP_NAME);
	if (test_name == NULL) {
		printf("filename failed \n");
		assert(0);
	}
	sprintf(test_name, "/tmp/%s", "testfileXXXXXX");

	test_fd = mkstemp(test_name);
	if (test_fd == -1) {
		free(test_name);
	}


	buf = (char *) calloc(1, TEST_BUF_SIZE);
	wsize = write(test_fd, buf, TEST_BUF_SIZE);
	assert(wsize == TEST_BUF_SIZE);
	close(test_fd);
	free(buf);

#ifdef	SUPPORT_O_DIRECT
	/*
	 * now test the file with direct flag 
	 */
	odisk->open_flags = (O_RDONLY | O_DIRECT);
#else
	/*
	 * now test the file with direct flag 
	 */
	odisk->open_flags = (O_RDONLY);
#endif


	test_fd = open(test_name, odisk->open_flags);
	assert(test_fd != -1);



	base = (char *) malloc(ALIGN_SIZE(TEST_BUF_SIZE));
	assert(base != NULL);
	data = (char *) ALIGN_VAL(base);

	wsize = read(test_fd, data, ALIGN_ROUND(TEST_BUF_SIZE));
	if (wsize != TEST_BUF_SIZE) {
		close(test_fd);
		test_fd = odisk->open_flags = O_RDONLY;
		assert(test_fd != -1);

		wsize = read(test_fd, data, ALIGN_ROUND(TEST_BUF_SIZE));
		assert(wsize != TEST_BUF_SIZE);
	}

	free(base);
	close(test_fd);
	unlink(test_name);
	free(test_name);
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

	sig_cal_init();

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
	dctl_register_leaf(DEV_OBJ_PATH, "dynamic_load",
			   DCTL_DT_UINT32, dctl_read_uint32,
			   dctl_write_uint32, &dynamic_load);
	dctl_register_leaf(DEV_OBJ_PATH, "dynamic_load_depth",
			   DCTL_DT_UINT32, dctl_read_uint32,
			   dctl_write_uint32, &dynamic_load_depth);

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

	odisk_setup_open_flags(new_state);

	for (i = 0; i < MAX_READ_THREADS; i++) {
		err = pthread_create(&new_state->thread_id, PATTR_DEFAULT,
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
odisk_continue()
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

#ifndef	XXX
static void
update_gid_idx(odisk_state_t * odisk, char *name, groupid_t * gid)
{
	char            idx_name[NAME_MAX];
	FILE           *idx_file;
	int             num;
	int             len;
	gid_idx_ent_t   gid_idx;

	len = snprintf(idx_name, NAME_MAX, "%s/%s%016llX",
		       odisk->odisk_indexdir, GID_IDX, *gid);
	assert(len < NAME_MAX);

	idx_file = fopen(idx_name, "a");
	if (idx_file == NULL) {
		fprintf(stderr, "update_gid_idx: failed to open <%s> \n",
			idx_name);
		return;
	}

	memset(&gid_idx, 0, sizeof(gid_idx));
	len = snprintf(gid_idx.gid_name, NAME_MAX, "%s", name);
	assert(len < NAME_MAX);

	num = fwrite(&gid_idx, sizeof(gid_idx), 1, idx_file);
	assert(num == 1);

	fclose(idx_file);
}
#else
static void
update_gid_idx(odisk_state_t * odisk, char *name, groupid_t * gid)
{
	char            new_name[NAME_MAX];
	char            old_name[NAME_MAX];
	FILE           *old_file;
	FILE           *new_file;
	int             num;
	gid_idx_ent_t   gid_idx;
	gid_idx_ent_t   cur_gididx;
	int             err;
	int             len;
	int             skip_copy = 0;

	len = snprintf(old_name, NAME_MAX, "%s/%s%016llX.old",
		       odisk->odisk_indexdir, GID_IDX, *gid);
	assert(len < NAME_MAX);
	len = snprintf(new_name, NAME_MAX, "%s/%s%016llX",
		       odisk->odisk_indexdir, GID_IDX, *gid);
	assert(len < NAME_MAX);


	/*
	 * rename the old index file
	 */
	err = rename(new_name, old_name);
	if (err) {
		perror("Failed renaming index file:");
		// return;
	}

	old_file = fopen(old_name, "r");
	if (old_file == NULL) {
		fprintf(stderr, "remove_gid_from_idx: failed to open <%s> \n",
			old_name);
		skip_copy = 1;
	}

	new_file = fopen(new_name, "w+");
	if (new_file == NULL) {
		fprintf(stderr, "remove_gid_from_idx: failed to open <%s> \n",
			new_name);
		return;
	}

	err = unlink(old_name);
	if (err) {
		perror("removing old file ");
		/*
		 * XXX what to do ....
		 */
	}

	/*
	 * write the new entry 
	 */
	memset(&gid_idx, 0, sizeof(gid_idx));
	len = snprintf(gid_idx.gid_name, NAME_MAX, "%s", name);
	assert(len < NAME_MAX);

	num = fwrite(&gid_idx, sizeof(gid_idx), 1, new_file);
	assert(num == 1);

	if (skip_copy)
		goto done;

	/*
	 * copy over the old entries 
	 */
	while (fread(&cur_gididx, sizeof(cur_gididx), 1, old_file) == 1) {
		num = fwrite(&cur_gididx, sizeof(cur_gididx), 1, new_file);
		if (num != 1) {
			perror("Failed writing odisk index: ");
		}
	}
	fclose(old_file);
done:
	fclose(new_file);
}
#endif

static void
remove_gid_from_idx(odisk_state_t * odisk, char *name, groupid_t * gid)
{
	char            new_name[NAME_MAX];
	char            old_name[NAME_MAX];
	FILE           *old_file;
	FILE           *new_file;
	int             num;
	gid_idx_ent_t   rem_gid_idx;
	gid_idx_ent_t   cur_gididx;
	int             err;
	int             len;

	len = snprintf(old_name, NAME_MAX, "%s/%s%016llX.old",
		       odisk->odisk_indexdir, GID_IDX, *gid);
	assert(len < NAME_MAX);
	len = snprintf(new_name, NAME_MAX, "%s/%s%016llX",
		       odisk->odisk_indexdir, GID_IDX, *gid);
	assert(len < NAME_MAX);


	/*
	 * rename the old index file 
	 */
	err = rename(new_name, old_name);
	if (err) {
		perror("Failed renaming index file:");
		return;
	}


	old_file = fopen(old_name, "r");
	if (old_file == NULL) {
		fprintf(stderr, "remove_gid_from_idx: failed to open <%s> \n",
			old_name);
		return;
	}

	new_file = fopen(new_name, "w+");
	if (new_file == NULL) {
		fprintf(stderr, "remove_gid_from_idx: failed to open <%s> \n",
			new_name);
		return;
	}

	err = unlink(old_name);
	if (err) {
		perror("removing old file ");
		/*
		 * XXX what to do .... 
		 */
	}

	memset(&rem_gid_idx, 0, sizeof(rem_gid_idx));
	snprintf(rem_gid_idx.gid_name, NAME_MAX, "%s", name);

	while (fread(&cur_gididx, sizeof(cur_gididx), 1, old_file) == 1) {
		if (memcmp(&cur_gididx, &rem_gid_idx, sizeof(cur_gididx)) ==
		    0) {
			continue;
		}
		num = fwrite(&cur_gididx, sizeof(cur_gididx), 1, new_file);
		if (num != 1) {
			perror("Failed writing odisk index: ");
		}
	}

	fclose(old_file);
	fclose(new_file);
}

static void
update_object_gids(odisk_state_t * odisk, obj_data_t * obj, char *name)
{
	gid_list_t     *glist;
	size_t          len;
	int             i,
	                err;

	len = 0;
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
	if (err != ENOMEM) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "update_object_gids: failed to read attr");
		return;
	}

	glist = (gid_list_t *) malloc(len);
	assert(glist != NULL);
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len,
			    (unsigned char *) glist);
	assert(err == 0);

	for (i = 0; i < glist->num_gids; i++) {
		if (glist->gids[i] == 0) {
			continue;
		}
		update_gid_idx(odisk, name, &glist->gids[i]);
	}

	free(glist);
}

static void
delete_object_gids(odisk_state_t * odisk, obj_data_t * obj)
{
	gid_list_t     *glist;
	size_t          len;
	int             i,
	                err;
	char            buf[NAME_MAX];
	int             slen;

	slen = snprintf(buf, NAME_MAX, "OBJ%016llX", obj->local_id);
	if (slen >= NAME_MAX) {
		log_message(LOGT_DISK, LOGL_ERR,
	   	    "delete_object_gids: attr file (%s) exceeds NAM_MAX", buf);
		return;
	}

	len = 0;
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME, &len, NULL);
	if (err != ENOMEM) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "delete_object_gids: failed to read attr");
		return;
	}

	glist = (gid_list_t *) malloc(len);
	assert(glist != NULL);
	err = obj_read_attr(&obj->attr_info, GIDLIST_NAME,
			    &len, (unsigned char *) glist);
	assert(err == 0);

	for (i = 0; i < glist->num_gids; i++) {
		if (glist->gids[i] == 0) {
			continue;
		}
		remove_gid_from_idx(odisk, buf, &glist->gids[i]);
	}

	free(glist);
}



/*
 * Clear all of the GID index files.  
 */

int
odisk_clear_indexes(odisk_state_t * odisk)
{
	struct dirent  *cur_ent;
	int             extlen, flen;
	char            idx_name[NAME_MAX];
	char           *poss_ext;
	DIR            *dir;
	int             count = 0;
	int             err,
	                len;

	dir = opendir(odisk->odisk_indexdir);
	if (dir == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_clear_indexes: uname to opendir (%s)",
		    odisk->odisk_indexdir);
		return (0);
	}


	while (1) {

		cur_ent = readdir(dir);
		/*
		 * If readdir fails, then we have enumerated all
		 * the contents.
		 */

		if (cur_ent == NULL) {
			closedir(dir);
			return (count);
		}

		/*
		 * If this isn't a file then we skip the entry.
		 */
		if ((cur_ent->d_type != DT_REG)
		    && (cur_ent->d_type != DT_LNK)) {
			continue;
		}

		/*
		 * If this begins with the prefix GID_IDX, then
		 * it is an index file so it should be deleted.
		 */

		flen = strlen(cur_ent->d_name);
		extlen = strlen(GID_IDX);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
				len = snprintf(idx_name, NAME_MAX, "%s/%s",
					       odisk->odisk_indexdir,
					       cur_ent->d_name);
				assert(len < NAME_MAX);
				err = remove(idx_name);
				if (err == -1) {
					fprintf(stderr,
						"Failed to remove %s\n",
						idx_name);
				}
			}
		}
	}

	closedir(dir);
}

int
odisk_build_indexes(odisk_state_t * odisk)
{
	struct dirent  *cur_ent;
	int             extlen,
	                flen;
	char           *poss_ext;
	char            max_path[NAME_MAX];
	DIR            *dir;
	int             count = 0;
	int             err,
	                len;
	obj_data_t     *new_object;

	dir = opendir(odisk->odisk_dataroot);
	if (dir == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_build_indexes: uname to opendir (%s)",
		    odisk->odisk_dataroot);
		return (0);
	}


	while (1) {

		cur_ent = readdir(dir);
		/*
		 * If readdir fails, then we have enumerated all
		 * the contents.
		 */

		if (cur_ent == NULL) {
			closedir(dir);
			return (count);
		}

		/*
		 * If this isn't a file then we skip the entry.
		 */
		if ((cur_ent->d_type != DT_REG)) {
			continue;
		}

		/*
		 * make sure this isn't an index file 
		 */
		flen = strlen(cur_ent->d_name);
		extlen = strlen(GID_IDX);
		if (flen > extlen) {
			if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
				continue;
			}
		}

		/*
		 * see if this is an attribute file 
		 */
		extlen = strlen(BIN_ATTR_EXT);
		flen = strlen(cur_ent->d_name);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strcmp(poss_ext, BIN_ATTR_EXT) == 0) {
				continue;
			}
		}

		extlen = strlen(CACHE_EXT);
		flen = strlen(cur_ent->d_name);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strcmp(poss_ext, CACHE_EXT) == 0) {
				continue;
			}
		}


		len = snprintf(max_path, NAME_MAX, "%s/%s",
		    odisk->odisk_dataroot, cur_ent->d_name);
		if (len < NAME_MAX) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "odisk_build_indexes: name (%s) exceeds NAME_MAX",
			    max_path);
			continue;
		}

		err = odisk_load_obj(odisk, &new_object, max_path);
		if (err) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "odisk_build_indexes: failed to load (%s)",
			    max_path);
			continue;
		}

		/*
		 * Go through each of the GID's and update the index file.
		 */
		update_object_gids(odisk, new_object, cur_ent->d_name);
		odisk_release_obj(new_object);
	}

	closedir(dir);
}


int
odisk_write_oids(odisk_state_t * odisk, uint32_t devid)
{
	struct dirent  *cur_ent;
	int             extlen,
	                flen;
	char           *poss_ext;
	char            max_path[NAME_MAX];
	DIR            *dir;
	int             count = 0;
	int             err;
	obj_id_t        obj_id;
	obj_data_t     *new_object;
	int             len;

	obj_id.dev_id = (uint64_t) devid;

	dir = opendir(odisk->odisk_dataroot);
	if (dir == NULL) {
		log_message(LOGT_DISK, LOGL_ERR,
		    "odisk_write_oids: open data dir (%s) failed",
		    odisk->odisk_dataroot);
		return (0);
	}


	while (1) {

		cur_ent = readdir(dir);
		/*
		 * If readdir fails, then we have enumerated all
		 * the contents.
		 */

		if (cur_ent == NULL) {
			closedir(dir);
			return (count);
		}

		/*
		 * If this isn't a file then we skip the entry.
		 */
		if ((cur_ent->d_type != DT_REG)
		    && (cur_ent->d_type != DT_LNK)) {
			continue;
		}

		/*
		 * make sure this isn't an index file 
		 */
		flen = strlen(cur_ent->d_name);
		extlen = strlen(GID_IDX);
		if (flen > extlen) {
			if (strncmp(cur_ent->d_name, GID_IDX, extlen) == 0) {
				continue;
			}
		}

		/*
		 * see if this is an attribute file 
		 */
		extlen = strlen(BIN_ATTR_EXT);
		flen = strlen(cur_ent->d_name);
		if (flen > extlen) {
			poss_ext = &cur_ent->d_name[flen - extlen];
			if (strcmp(poss_ext, BIN_ATTR_EXT) == 0) {
				continue;
			}
		}

		len = snprintf(max_path, NAME_MAX, "%s/%s",
		    odisk->odisk_dataroot, cur_ent->d_name);
		if (len >= NAME_MAX) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "odisk_write_oids: file name <%s> exceeds NAME_MAX",
			    max_path);
			continue;
		}

		err = odisk_load_obj(odisk, &new_object, max_path);
		if (err) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "odisk_write_oids: file name <%s> load fails",
			    max_path);
			continue;
		}
		obj_id.local_id = new_object->local_id;

		err = obj_write_attr(&new_object->attr_info, "MY_OID",
		    sizeof(obj_id), (unsigned char *) &obj_id);
		if (err != 0) {
			log_message(LOGT_DISK, LOGL_ERR,
		    	    "odisk_write_oids: file name <%s> load fails",
			    max_path);
			continue;
		}

		/*
		 * save the state 
		 */
		odisk_save_obj(odisk, new_object);
		odisk_release_obj(new_object);
	}
	closedir(dir);
}

/*
 * Create "special" null object that indicates the end of a data
 * stream.  This is a bit of a hack, but ...
 */

obj_data_t     *
odisk_null_obj()
{
	obj_data_t     *new_obj;

	new_obj = (obj_data_t *) malloc(sizeof(*new_obj));
	assert(new_obj != NULL);

	new_obj->data_len = 0;
	new_obj->data = NULL;
	new_obj->base = NULL;
	new_obj->ref_count = 1;
	pthread_mutex_init(&new_obj->mutex, NULL);

	new_obj->attr_info.attr_ndata = 0;
	new_obj->attr_info.attr_dlist = NULL;

	return (new_obj);
}
