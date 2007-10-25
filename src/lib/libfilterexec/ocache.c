/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
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
#include <stddef.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include <sys/uio.h>
#include <limits.h>

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "ocache_priv.h"
#include "obj_attr.h"
#include "obj_attr.h"
#include "lib_filter.h"
#include "lib_filter_sys.h"
#include "lib_odisk.h"
#include "lib_filterexec.h"
#include "lib_ocache.h"
#include "lib_dconfig.h"
#include "filter_priv.h"


#define	MAX_FNAME	128
#define TEMP_ATTR_BUF_SIZE	1024

/*
 * dctl variables 
 */
static unsigned int    if_cache_table = 1;
static unsigned int    if_cache_oattr = 0;

static int      search_active = 0;
static int      search_done = 0;

static GAsyncQueue *ocache_queue;

struct ocache_start_entry {
	void *cache_table;	/* ocache */
	sig_val_t fsig;		/* oattr - filter signature */
};

struct ocache_attr_entry {
	obj_data_t *	obj;
	unsigned int	name_len;
	char		name[MAX_ATTR_NAME];
};

struct ocache_end_entry {
	int result;
	query_info_t qid;		/* search that created entry */
	filter_exec_mode_t exec_mode;	/* mode when entry was created */
};

struct ocache_ring_entry {
	int type;
	sig_val_t id_sig;
	union {
		struct ocache_start_entry start;/* INSERT_START */
		cache_attr_entry iattr;		/* INSERT_IATTR */
		struct ocache_attr_entry oattr;	/* INSERT_OATTR */
		struct ocache_end_entry end;	/* INSERT_END */
	} u;
};

static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 * active 
 */
static pthread_cond_t bg_active_cv = PTHREAD_COND_INITIALIZER;


#define SIG_BUF_SIZE	256
#define MAX_FILTER_ARG_NAME 256
#define CACHE_ENTRY_NUM 4096
#define FCACHE_NUM 50
#define MAX_CACHE_ENTRY_NUM 0X1000000
#define MAX_ENTRY_NUM	(2 * MAX_CACHE_ENTRY_NUM)
#define MAX_IATTR_SIZE	4096

static cache_init_obj **init_table;
static fcache_t *filter_cache_table[FCACHE_NUM];
static int      cache_entry_num = 0;	/* for debug purpose */

static sig_val_t ocache_sig = { {0,} };

/* wrappers around g_async_queue_push and pop to get type checking */
static void ocache_queue_push(struct ocache_ring_entry *data)
{
	g_async_queue_push(ocache_queue, (gpointer)data);
}

static struct ocache_ring_entry *ocache_queue_pop(void)
{
	return (struct ocache_ring_entry *)g_async_queue_pop(ocache_queue);
}

/*
 * This could be moved to a support library XXX
 */
int
digest_cal(filter_data_t * fdata, char *fn_name, int numarg, char **filt_args,
	   int blob_len, void *blob, sig_val_t * signature)
{
	struct ciovec *iov;
	unsigned int i, len, n = 0;

	len =	fdata->num_libs +	/* library_signatures */
		1 +			/* filter name */
		numarg +		/* filter arguments */
		1;			/* optional binary blob */

	iov = (struct ciovec *)malloc(len * sizeof(struct ciovec));
	assert(iov != NULL);

	/* include the library signatures */
	for (i = 0; i < fdata->num_libs; i++) {
		iov[n].iov_base = &fdata->lib_info[i].lib_sig;
		iov[n].iov_len = sizeof(sig_val_t);
		n++;
	}

	iov[n].iov_base = fn_name;
	iov[n].iov_len = strlen(fn_name);
	n++;

	/* include the args */
	for (i = 0; i < numarg; i++) {
		if (filt_args[i] == NULL)
			continue;

		len = strlen(filt_args[i]);
		if (len >= MAX_FILTER_ARG_NAME)
			return (EINVAL);

		iov[n].iov_base = filt_args[i];
		iov[n].iov_len = len;
		n++;
	}

	/* include binary blob */
	if (blob_len > 0) {
		iov[n].iov_base = blob;
		iov[n].iov_len = blob_len;
		n++;
	}

	sig_cal_vec(iov, n, signature);
	free(iov);
	return 0;
}

/*
 * This is only used in this file, maybe there is a duplicate implementation
 * in another places?
 */
static void
sig_iattr(cache_attr_set * iattr, sig_val_t * sig)
{
	struct ciovec *iov;
	int i;

	iov = (struct ciovec *)malloc(iattr->entry_num * sizeof(struct ciovec));
	assert(iov != NULL);

	for (i = 0; i < iattr->entry_num; i++) {
		iov[i].iov_base = iattr->entry_data[i];
		iov[i].iov_len = sizeof(cache_attr_entry);
	}

	sig_cal_vec(iov, iattr->entry_num, sig);
	free(iov);
}

static int
attr_in_set(cache_attr_entry * inattr, cache_attr_set * set)
{
	int             j;
	cache_attr_entry *tattr;

	for (j = 0; j < set->entry_num; j++) {
		tattr = set->entry_data[j];
		if (tattr == NULL) {
			printf("null temp_j, something wrong\n");
			continue;
		}
		if (memcmp(inattr, tattr, sizeof(*tattr)) == 0) {
			return (1);
		}
	}
	return (0);

}

/*
 * This to see if attr1 is a strict subset of attr2, if not, then we
 * return 1, otherwise return 0.
 */

static int
compare_attr_set(cache_attr_set * attr1, cache_attr_set * attr2)
{
	int             i;
	cache_attr_entry *temp_i;

	/*
	 * for each item in attr1, see if it exists in attr2.
	 */
	for (i = 0; i < attr1->entry_num; i++) {
		temp_i = attr1->entry_data[i];
		if (temp_i == NULL) {
			printf("null temp_i, something wrong\n");
			continue;
		}

		/*
		 * if this item isn't in the set then return 1 
		 */
		if (attr_in_set(temp_i, attr2) == 0) {
			return 1;
		}
	}
	return 0;
}

int
combine_attr_set(cache_attr_set * attr1, cache_attr_set * attr2)
{
	int             i,
	                j;
	int             found;
	cache_attr_entry *temp_i,
	               *temp_j;
	cache_attr_entry **tmp;

	for (i = 0; i < attr2->entry_num; i++) {
		temp_i = attr2->entry_data[i];
		if (temp_i == NULL) {
			printf("null temp_i, something wrong\n");
			assert(0);
		}
		found = 0;
		for (j = 0; j < attr1->entry_num; j++) {
			temp_j = attr1->entry_data[j];
			if (temp_j == NULL) {
				printf("null temp_j, something wrong\n");
				break;
			}
			if (memcmp(temp_i, temp_j, sizeof(*temp_j)) == 0) {
				attr1->entry_data[j] = temp_i;
				found = 1;
				break;
			}
		}
		/*
		 * no found, add to the tail 
		 */
		if (!found) {
			attr1->entry_data[attr1->entry_num] = temp_i;
			attr1->entry_num++;
			if ((attr1->entry_num % ATTR_ENTRY_NUM) == 0) {
				tmp = calloc(1, (attr1->entry_num +
						 ATTR_ENTRY_NUM) *
					     sizeof(char *));
				assert(tmp != NULL);
				memcpy(tmp, attr1->entry_data,
				       attr1->entry_num * sizeof(char *));
				free(attr1->entry_data);
				attr1->entry_data = tmp;
			}
		}
	}
	return 0;
}

static int
ocache_entry_free(cache_obj * cobj)
{
	int             i;

	if (cobj == NULL) {
		return (0);
	}

	for (i = 0; i < cobj->iattr.entry_num; i++) {
		if (cobj->iattr.entry_data[i] != NULL) {
			free(cobj->iattr.entry_data[i]);
		}
	}

	if (cobj->iattr.entry_data != NULL) {
		free(cobj->iattr.entry_data);
	}
	for (i = 0; i < cobj->oattr.entry_num; i++) {
		if (cobj->oattr.entry_data[i] != NULL) {
			free(cobj->oattr.entry_data[i]);
		}
	}
	if (cobj->oattr.entry_data != NULL) {
		free(cobj->oattr.entry_data);
	}
	free(cobj);
	return (0);
}

/*
 * Build state to keep track of initial object attributes.
 */
void
cache_set_init_attrs(sig_val_t * id_sig, obj_attr_t * init_attr)
{
	cache_init_obj *cobj;
	unsigned int    index;
	attr_record_t  *arec;
	unsigned char  *buf;
	size_t          len;
	void           *cookie;
	int             err;
	cache_attr_entry *attr_entry;


	pthread_mutex_lock(&shared_mutex);
	index = sig_hash(id_sig) % CACHE_ENTRY_NUM;

	for (cobj = init_table[index]; cobj != NULL; cobj = cobj->next) {
		if (sig_match(&cobj->id_sig, id_sig)) {
			pthread_mutex_unlock(&shared_mutex);
			return;
		}
	}

	cobj = (cache_init_obj *) calloc(1, sizeof(*cobj));
	memcpy(&cobj->id_sig, id_sig, sizeof(sig_val_t));
	cobj->attr.entry_num = 0;
	cobj->attr.entry_data = calloc(1, ATTR_ENTRY_NUM * sizeof(char *));
	assert(cobj->attr.entry_data != NULL);

	err = obj_get_attr_first(init_attr, &buf, &len, &cookie, 0);
	while (err != ENOENT) {
		if (buf == NULL) {
			printf("can not get attr\n");
			break;
		}
		arec = (attr_record_t *) buf;
		attr_entry = (cache_attr_entry *)
		    calloc(1, sizeof(cache_attr_entry));
		assert(attr_entry != NULL);

		attr_entry->name_len = arec->name_len;

		memcpy(attr_entry->the_attr_name, arec->data, arec->name_len);
		memcpy(&attr_entry->attr_sig, &arec->attr_sig,
		       sizeof(sig_val_t));
		cobj->attr.entry_data[cobj->attr.entry_num] = attr_entry;
		cobj->attr.entry_num++;
		if ((cobj->attr.entry_num % ATTR_ENTRY_NUM) == 0) {
			cache_attr_entry **tmp;
			tmp = calloc(1, (cobj->attr.entry_num +
					 ATTR_ENTRY_NUM) * sizeof(char *));
			assert(tmp != NULL);
			memcpy(tmp, cobj->attr.entry_data,
			       cobj->attr.entry_num * sizeof(char *));
			free(cobj->attr.entry_data);
			cobj->attr.entry_data = tmp;
		}
		err = obj_get_attr_next(init_attr, &buf, &len, &cookie, 0);
	}


	cobj->next = init_table[index];
	init_table[index] = cobj;
	pthread_mutex_unlock(&shared_mutex);
	return;
}


int
cache_get_init_attrs(sig_val_t * id_sig, cache_attr_set * init_set)
{
	cache_init_obj *cobj;
	int             found = 0;
	unsigned int    index;

	pthread_mutex_lock(&shared_mutex);
	index = sig_hash(id_sig) % CACHE_ENTRY_NUM;
	for (cobj = init_table[index]; cobj != NULL; cobj = cobj->next) {
		if (sig_match(&cobj->id_sig, id_sig)) {
			found = 1;
			init_set->entry_num = cobj->attr.entry_num;
			/*
			 * XXXLH this can bee too big ??? 
			 */
			memcpy(init_set->entry_data, cobj->attr.entry_data,
			       cobj->attr.entry_num * sizeof(char *));
			break;
		}
	}

	pthread_mutex_unlock(&shared_mutex);
	return found;
}


int
cache_lookup(sig_val_t * id_sig, sig_val_t * fsig, void *fcache_table,
	     cache_attr_set * change_attr, int *err,
	     cache_attr_set ** oattr_set, sig_val_t * iattr_sig, 
	     query_info_t *qinfo)
{
	cache_obj      *cobj;
	int             found = 0;
	unsigned int    index;
	cache_obj     **cache_table = (cache_obj **) fcache_table;

	if (cache_table == NULL)
		return found;
	if (search_done == 1) {
		return (ENOENT);
	}
	pthread_mutex_lock(&shared_mutex);
	index = sig_hash(id_sig) % CACHE_ENTRY_NUM;
	cobj = cache_table[index];
	sig_clear(iattr_sig);

	/*
	 * cache hit if there is a (id_sig, filter sig, input attr sig) match 
	 */
	while (cobj != NULL) {
		if (sig_match(&cobj->id_sig, id_sig)) {
			/*
			 * compare change_attr set with input attr set 
			 */
			if (!compare_attr_set(&cobj->iattr, change_attr)) {
				found = 1;
				*err = cobj->result;
				cobj->ahit_count++;

				memcpy(iattr_sig, &cobj->iattr_sig,
				       sizeof(sig_val_t));
				memcpy(qinfo, &cobj->qid, sizeof(query_info_t));
				
				/*
				 * pass back the output attr set 
				 */
				*oattr_set = &cobj->oattr;
				break;
			}
		}
		cobj = cobj->next;
	}

	pthread_mutex_unlock(&shared_mutex);

	return found;
}

static int
time_after(struct timeval *time1, struct timeval *time2)
{
	if (time1->tv_sec > time2->tv_sec)
		return (1);
	if (time1->tv_sec < time2->tv_sec)
		return (0);
	if (time1->tv_usec > time2->tv_usec)
		return (1);
	return (0);
}

static int
ocache_update(int fd, cache_obj ** cache_table, struct stat *stats)
{
	cache_obj      *cobj;
	int             i;
	off_t           size,
	                rsize;
	cache_obj      *p,
	               *q;
	unsigned int    index;
	int             duplicate;

	if (fd < 0) {
		printf("cache file does not exist\n");
		return (EINVAL);
	}
	size = stats->st_size;
	rsize = 0;

	pthread_mutex_lock(&shared_mutex);
	while (rsize < size) {
		cobj = (cache_obj *) calloc(1, sizeof(*cobj));
		assert(cobj != NULL);
		read(fd, &cobj->id_sig, sizeof(sig_val_t));
		read(fd, &cobj->iattr_sig, sizeof(sig_val_t));
		read(fd, &cobj->result, sizeof(int));

		read(fd, &cobj->eval_count, sizeof(unsigned short));
		cobj->aeval_count = 0;
		read(fd, &cobj->hit_count, sizeof(unsigned short));
		cobj->ahit_count = 0;
		rsize +=
		    (2 * sizeof(sig_val_t) + sizeof(int) +
		     2 * sizeof(unsigned short));
		     
		read(fd, &cobj->qid, sizeof(query_info_t));
		read(fd, &cobj->exec_mode, sizeof(filter_exec_mode_t));
		rsize += sizeof(query_info_t) + sizeof(filter_exec_mode_t);
		
		read(fd, &cobj->iattr.entry_num, sizeof(unsigned int));
		rsize += sizeof(unsigned int);
		cobj->iattr.entry_data =
		    calloc(1, cobj->iattr.entry_num * sizeof(char *));
		assert(cobj->iattr.entry_data != NULL);
		for (i = 0; i < cobj->iattr.entry_num; i++) {
			cobj->iattr.entry_data[i] =
			    calloc(1, sizeof(cache_attr_entry));
			assert(cobj->iattr.entry_data[i] != NULL);
			rsize += read(fd, cobj->iattr.entry_data[i],
				      sizeof(cache_attr_entry));
		}

		read(fd, &cobj->oattr.entry_num, sizeof(unsigned int));
		rsize += sizeof(unsigned int);
		cobj->oattr.entry_data =
		    calloc(1, cobj->oattr.entry_num * sizeof(char *));
		assert(cobj->oattr.entry_data != NULL);
		for (i = 0; i < cobj->oattr.entry_num; i++) {
			cobj->oattr.entry_data[i] =
			    calloc(1, sizeof(cache_attr_entry));
			rsize +=
			    read(fd, cobj->oattr.entry_data[i],
				 sizeof(cache_attr_entry));
		}
		cobj->next = NULL;
		/*
		 * insert it into the cache_table array 
		 */
		index = sig_hash(&cobj->id_sig) % CACHE_ENTRY_NUM;
		if (cache_table[index] == NULL) {
			cache_table[index] = cobj;
			/*
			 * for debug purpose 
			 */
			cache_entry_num++;
		} else {
			p = cache_table[index];
			q = p;
			duplicate = 0;
			while (p != NULL) {
				if (sig_match(&p->id_sig, &cobj->id_sig) &&
				    sig_match(&p->iattr_sig,
					      &cobj->iattr_sig)) {
					if (cobj->eval_count > p->eval_count) {
						p->eval_count +=
						    (cobj->eval_count -
						     p->eval_count);
					}
					if (cobj->hit_count > p->hit_count) {
						p->hit_count +=
						    (cobj->hit_count -
						     p->hit_count);
					}
					duplicate = 1;
					break;
				}
				q = p;
				p = p->next;
			}
			if (duplicate) {
				ocache_entry_free(cobj);
			} else {
				q->next = cobj;
				/*
				 * for debug purpose 
				 */
				cache_entry_num++;
			}
		}
	}
	pthread_mutex_unlock(&shared_mutex);
	return (0);
}

static int
ocache_write_file(char *disk_path, fcache_t * fcache)
{
	char            fpath[PATH_MAX];
	cache_obj      *cobj;
	cache_obj      *tmp;
	int             i,
	                j;
	int             fd;
	int             err;
	cache_obj     **cache_table;
	unsigned int    count;
	struct stat     stats;
	char           *s_str;

	assert(fcache != NULL);
	cache_table = (cache_obj **) fcache->cache_table;
	s_str = sig_string(&fcache->fsig);
	if (s_str == NULL) {
		return (0);
	}
	sprintf(fpath, "%s/%s.%s", disk_path, s_str, CACHE_EXT);
	free(s_str);

	fd = open(fpath, O_RDWR, 00777);
	if (fd >=  0) {
		err = flock(fd, LOCK_EX);
		if (err) {
			perror("failed to lock cache file\n");
			close(fd);
			return (0);
		}
		err = fstat(fd, &stats);
		if (err != 0) {
			perror("failed to stat cache file\n");
			close(fd);
			return (0);
		}
		if (memcmp(&stats.st_mtime, &fcache->mtime, sizeof(time_t))) {
			err = ocache_update(fd, cache_table, &stats);
		}
		close(fd);
	}

	fd = open(fpath, O_CREAT | O_RDWR | O_TRUNC, 00777);
	err = flock(fd, LOCK_EX);
	if (err) {
		perror("failed to lock cache file\n");
		close(fd);
		return (0);
	}

	pthread_mutex_lock(&shared_mutex);
	for (j = 0; j < CACHE_ENTRY_NUM; j++) {
		cobj = cache_table[j];
		while (cobj != NULL) {
			write(fd, &cobj->id_sig, sizeof(sig_val_t));
			write(fd, &cobj->iattr_sig, sizeof(sig_val_t));
			write(fd, &cobj->result, sizeof(int));

			count = cobj->eval_count + cobj->aeval_count;
			write(fd, &count, sizeof(unsigned short));
			count = cobj->hit_count + cobj->ahit_count;
			write(fd, &count, sizeof(unsigned short));
			
			write(fd, &cobj->qid, sizeof(query_info_t));
			write(fd, &cobj->exec_mode, sizeof(filter_exec_mode_t));
			
			write(fd, &cobj->iattr.entry_num,
			      sizeof(unsigned int));
			for (i = 0; i < cobj->iattr.entry_num; i++) {
				write(fd, cobj->iattr.entry_data[i],
				      sizeof(cache_attr_entry));
			}

			write(fd, &cobj->oattr.entry_num,
			      sizeof(unsigned int));

			for (i = 0; i < cobj->oattr.entry_num; i++) {
				write(fd, cobj->oattr.entry_data[i],
				      sizeof(cache_attr_entry));
			}

			tmp = cobj;
			cobj = cobj->next;
			/*
			 * free 
			 */
			ocache_entry_free(tmp);
			cache_entry_num--;
		}
	}
	for (i = 0; i < CACHE_ENTRY_NUM; i++) {
		cache_table[i] = NULL;
	}
	pthread_mutex_unlock(&shared_mutex);
	close(fd);
	return (0);
}

/*
 * free cache table for unused filters: a very simple LRU 
 */
static int
free_fcache_entry(char *disk_path)
{
	int             i;
	fcache_t       *oldest = NULL;
	int             found = -1;

	do {
		for (i = 0; i < FCACHE_NUM; i++) {
			if (filter_cache_table[i] == NULL)
				continue;
			if (filter_cache_table[i]->running > 0)
				continue;
			if (oldest == NULL) {
				oldest = filter_cache_table[i];
				found = i;
			} else {
				if (time_after
				    (&oldest->atime,
				     &filter_cache_table[i]->atime)) {
					oldest = filter_cache_table[i];
					found = i;
				}
			}
		}
		if (oldest == NULL) {
			return (-1);
		} else {
			ocache_write_file(disk_path, oldest);
			free(oldest->cache_table);
			free(oldest);
			filter_cache_table[found] = NULL;
			oldest = NULL;
		}
	}
	while (cache_entry_num >= MAX_CACHE_ENTRY_NUM);

	return (found);
}

static int
ocache_init_read(char *disk_path)
{
	char            fpath[PATH_MAX];
	cache_init_obj *cobj;
	off_t           size,
	                rsize;
	int             fd;
	struct stat     stats;
	cache_init_obj *p,
	               *q;
	unsigned int    index;
	int             i,
	                err;

	sprintf(fpath, "%s/ATTRSIG", disk_path);

	init_table =
	    (cache_init_obj **) calloc(1, sizeof(char *) * CACHE_ENTRY_NUM);
	assert(init_table != NULL);
	for (i = 0; i < CACHE_ENTRY_NUM; i++) {
		init_table[i] = NULL;
	}

	fd = open(fpath, O_RDONLY, 00777);
	if (fd < 0) {
		return (0);
	}

	err = flock(fd, LOCK_EX);
	if (err != 0) {
		perror("failed to lock cache file\n");
		close(fd);
		return (0);
	}
	err = fstat(fd, &stats);
	if (err != 0) {
		perror("failed to stat cache file\n");
		close(fd);
		return (EINVAL);
	}
	size = stats.st_size;

	rsize = 0;
	pthread_mutex_lock(&shared_mutex);
	while (rsize < size) {
		cobj = (cache_init_obj *) calloc(1, sizeof(*cobj));
		assert(cobj != NULL);
		read(fd, &cobj->id_sig, sizeof(sig_val_t));
		read(fd, &cobj->attr.entry_num, sizeof(unsigned int));
		rsize += (sizeof(sig_val_t) + sizeof(unsigned int));
		cobj->attr.entry_data =
		    calloc(1, cobj->attr.entry_num * sizeof(char *));
		assert(cobj->attr.entry_data != NULL);


		for (i = 0; i < cobj->attr.entry_num; i++) {
			cobj->attr.entry_data[i] =
			    calloc(1, sizeof(cache_attr_entry));
			assert(cobj->attr.entry_data[i] != NULL);
			rsize += read(fd, cobj->attr.entry_data[i],
				      sizeof(cache_attr_entry));
		}

		cobj->next = NULL;
		index = sig_hash(&cobj->id_sig) % CACHE_ENTRY_NUM;
		if (init_table[index] == NULL) {
			init_table[index] = cobj;
		} else {
			p = init_table[index];
			while (p != NULL) {
				q = p;
				p = p->next;
			}
			q->next = cobj;
		}
	}
	pthread_mutex_unlock(&shared_mutex);
	close(fd);
	return (0);
}

static int
ocache_init_write(char *disk_path)
{
	char            fpath[PATH_MAX];
	cache_init_obj *cobj,
	               *tmp;
	int             fd;
	int             i,
	                j,
	                err;

	if (init_table == NULL) {
		return (0);
	}
	sprintf(fpath, "%s/ATTRSIG", disk_path);
	fd = open(fpath, O_CREAT | O_RDWR | O_TRUNC, 00777);
	err = flock(fd, LOCK_EX);
	if (err) {
		perror("failed to lock cache file\n");
		close(fd);
		return (0);
	}
	pthread_mutex_lock(&shared_mutex);
	for (j = 0; j < CACHE_ENTRY_NUM; j++) {
		cobj = init_table[j];
		while (cobj != NULL) {
			write(fd, &cobj->id_sig, sizeof(sig_val_t));
			write(fd, &cobj->attr.entry_num,
			      sizeof(unsigned int));
			for (i = 0; i < cobj->attr.entry_num; i++) {
				write(fd, cobj->attr.entry_data[i],
				      sizeof(cache_attr_entry));
			}
			tmp = cobj;
			cobj = cobj->next;

			for (i = 0; i < tmp->attr.entry_num; i++) {
				if (tmp->attr.entry_data[i] != NULL) {
					free(tmp->attr.entry_data[i]);
				}
			}
			if (tmp->attr.entry_data != NULL)
				free(tmp->attr.entry_data);
			free(tmp);
		}
	}
	pthread_mutex_unlock(&shared_mutex);
	close(fd);
	return (0);
}

int
ocache_read_file(char *disk_path, sig_val_t * fsig, void **fcache_table,
		 struct timeval *atime)
{
	char            fpath[PATH_MAX];
	cache_obj      *cobj;
	int             i;
	off_t           size;
	int             fd;
	struct stat     stats;
	cache_obj      *p,
	               *q;
	unsigned int    index;
	fcache_t       *fcache;
	int             err;
	char           *sig_str;
	int             duplicate;
	cache_obj     **cache_table;
	int             filter_cache_table_num = -1;
	int             rc;

	*fcache_table = NULL;

	/*
	 * lookup the filter in cached filter array 
	 */
	for (i = 0; i < FCACHE_NUM; i++) {
		if (filter_cache_table[i] == NULL)
			continue;

		if (sig_match(&filter_cache_table[i]->fsig, fsig)) {
			*fcache_table = filter_cache_table[i]->cache_table;
			memcpy(&filter_cache_table[i]->atime, atime,
			       sizeof(struct timeval));
			filter_cache_table[i]->running++;
			return (0);
		}
	}

	/*
	 * if not found, try to get a free entry for this filter 
	 */
	sig_str = sig_string(fsig);
	if (sig_str == NULL) {
		return (ENOENT);
	}
	/*
	 * XXX overflow on buffer
	 */
	sprintf(fpath, "%s/%s.%s", disk_path, sig_str, CACHE_EXT);
	free(sig_str);

	for (i = 0; i < FCACHE_NUM; i++) {
		if (filter_cache_table[i] == NULL) {
			filter_cache_table_num = i;
			break;
		}
	}

	if ((cache_entry_num > MAX_CACHE_ENTRY_NUM) ||
	    (filter_cache_table_num == -1)) {
		err = free_fcache_entry(disk_path);
		if (err < 0) {
			printf("can not find free fcache entry\n");
			return (ENOMEM);
		}
		filter_cache_table_num = err;
	}

	cache_table =
	    (cache_obj **) calloc(1, sizeof(char *) * CACHE_ENTRY_NUM);
	assert(cache_table != NULL);

	for (i = 0; i < CACHE_ENTRY_NUM; i++) {
		cache_table[i] = NULL;
	}

	fcache = (fcache_t *) calloc(1, sizeof(fcache_t));
	assert(fcache != NULL);

	filter_cache_table[filter_cache_table_num] = fcache;
	fcache->cache_table = (void *) cache_table;
	memcpy(&fcache->fsig, fsig, sizeof(sig_val_t));

	assert(atime != NULL);
	memcpy(&fcache->atime, atime, sizeof(struct timeval));
	fcache->running = 1;
	*fcache_table = (void *) cache_table;

	fd = open(fpath, O_RDONLY, 00777);
	if (fd < 0) {
		memset(&fcache->mtime, 0, sizeof(time_t));
		return (0);
	}
	err = flock(fd, LOCK_EX);
	if (err != 0) {
		perror("failed to lock cache file\n");
		close(fd);
		return (0);
	}
	err = fstat(fd, &stats);
	if (err != 0) {
		perror("failed to stat cache file\n");
		close(fd);
		return (EINVAL);
	}
	size = stats.st_size;
	memcpy(&fcache->mtime, &stats.st_mtime, sizeof(time_t));

	pthread_mutex_lock(&shared_mutex);
	while (1) {
		cobj = (cache_obj *) calloc(1, sizeof(*cobj));
		assert(cobj != NULL);
		rc = read(fd, &cobj->id_sig, sizeof(sig_val_t));
		if (rc == 0) {
			break;
		} else if (rc < 0) {
			printf("read error \n");
			break;
		}
		rc = read(fd, &cobj->iattr_sig, sizeof(sig_val_t));
		assert(rc == sizeof(sig_val_t));
		rc = read(fd, &cobj->result, sizeof(int));
		assert(rc == sizeof(int));

		rc = read(fd, &cobj->eval_count, sizeof(unsigned short));
		assert(rc == sizeof(unsigned short));
		cobj->aeval_count = 0;
		rc = read(fd, &cobj->hit_count, sizeof(unsigned short));
		assert(rc == sizeof(unsigned short));
		cobj->ahit_count = 0;

		rc = read(fd, &cobj->qid, sizeof(query_info_t));
		assert(rc == sizeof(query_info_t));
		rc = read(fd, &cobj->exec_mode, sizeof(filter_exec_mode_t));
		assert(rc == sizeof(filter_exec_mode_t));

		rc = read(fd, &cobj->iattr.entry_num, sizeof(unsigned int));
		assert(rc == sizeof(unsigned int));
		cobj->iattr.entry_data =
		    calloc(1, cobj->iattr.entry_num * sizeof(char *));
		assert(cobj->iattr.entry_data != NULL);
		for (i = 0; i < cobj->iattr.entry_num; i++) {
			cobj->iattr.entry_data[i] =
			    calloc(1, sizeof(cache_attr_entry));
			assert(cobj->iattr.entry_data[i] != NULL);

			rc = read(fd, cobj->iattr.entry_data[i],
				  sizeof(cache_attr_entry));
			assert(rc == sizeof(cache_attr_entry));

		}

		rc = read(fd, &cobj->oattr.entry_num, sizeof(unsigned int));
		assert(rc == sizeof(unsigned int));

		cobj->oattr.entry_data =
		    calloc(1, cobj->oattr.entry_num * sizeof(char *));
		assert(cobj->oattr.entry_data != NULL);

		for (i = 0; i < cobj->oattr.entry_num; i++) {
			cobj->oattr.entry_data[i] =
			    calloc(1, sizeof(cache_attr_entry));
			assert(cobj->oattr.entry_data[i] != NULL);

			rc = read(fd, cobj->oattr.entry_data[i],
				  sizeof(cache_attr_entry));
			assert(rc == sizeof(cache_attr_entry));
		}
		cobj->next = NULL;


		/*
		 * insert it into the cache_table array 
		 */
		index = sig_hash(&cobj->id_sig) % CACHE_ENTRY_NUM;
		if (cache_table[index] == NULL) {
			cache_table[index] = cobj;
			/*
			 * for debug purpose 
			 */
			cache_entry_num++;
		} else {
			p = cache_table[index];
			q = p;
			duplicate = 0;
			while (p != NULL) {
				if (sig_match(&p->id_sig, &cobj->id_sig) &&
				    sig_match(&p->iattr_sig,
					      &cobj->iattr_sig)) {
					duplicate = 1;
					break;
				}
				q = p;
				p = p->next;
			}
			if (duplicate) {
				ocache_entry_free(cobj);
			} else {
				q->next = cobj;
				/*
				 * for debug purpose 
				 */
				cache_entry_num++;
			}
		}
	}
	pthread_mutex_unlock(&shared_mutex);
	close(fd);
	return (0);
}

int
ocache_add_start(char *fhandle, sig_val_t * id_sig, void *cache_table,
		 sig_val_t * fsig)
{
	struct ocache_ring_entry *new_entry;

	memcpy(&ocache_sig, id_sig, sizeof(sig_val_t));

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_START;
	memcpy(&new_entry->id_sig, id_sig, sizeof(sig_val_t));
	memcpy(&new_entry->u.start.fsig, fsig, sizeof(sig_val_t));
	new_entry->u.start.cache_table = cache_table;

	ocache_queue_push(new_entry);
	return 0;
}

static void
ocache_add_iattr(lf_obj_handle_t ohandle,
		 const char *name, off_t len, const unsigned char *data)
{
	struct ocache_ring_entry *new_entry;
	obj_data_t *obj = (obj_data_t *) ohandle;

	if (!sig_match(&ocache_sig, &obj->id_sig))
		return;

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_IATTR;
	memcpy(&new_entry->id_sig, &obj->id_sig, sizeof(sig_val_t));
	new_entry->u.iattr.name_len = name ? strlen(name) + 1 : 0;
	strncpy(new_entry->u.iattr.the_attr_name, name, MAX_ATTR_NAME);
	odisk_get_attr_sig(obj, name, &new_entry->u.iattr.attr_sig);

	ocache_queue_push(new_entry);
}

static void
ocache_add_oattr(lf_obj_handle_t ohandle, const char *name,
		 off_t len, const unsigned char *data)
{
	struct ocache_ring_entry *new_entry;
	obj_data_t *obj = (obj_data_t *) ohandle;

	if (!sig_match(&ocache_sig, &obj->id_sig))
		return;

	/*
	 * call function to update stats 
	 */
	ceval_wattr_stats(len);

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_OATTR;
	memcpy(&new_entry->id_sig, &obj->id_sig, sizeof(sig_val_t));
	new_entry->u.oattr.obj = obj;
	new_entry->u.oattr.name_len = name ? strlen(name) + 1 : 0;
	strncpy(new_entry->u.oattr.name, name, MAX_ATTR_NAME);

	odisk_ref_obj(obj);

	ocache_queue_push(new_entry);
}

int
ocache_add_end(char *fhandle, sig_val_t * id_sig, int conf,
	       query_info_t *qid, filter_exec_mode_t exec_mode)
{
	struct ocache_ring_entry *new_entry;

	if (!sig_match(&ocache_sig, id_sig))
		return 0;

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_END;
	memcpy(&new_entry->id_sig, id_sig, sizeof(sig_val_t));
	new_entry->u.end.result = conf;
	new_entry->u.end.qid = *qid;
	new_entry->u.end.exec_mode = exec_mode;

	sig_clear(&ocache_sig);

	ocache_queue_push(new_entry);
	return 0;
}

#define	MAX_VEC_SIZE	10

static void    *
ocache_main(void *arg)
{
	ocache_state_t *cstate = (ocache_state_t *) arg;
	struct ocache_ring_entry *tobj;

	/* ocache objects */
	cache_obj      *cobj;
	int             correct;
	cache_obj     **cache_table;
	cache_attr_entry **tmp, *src_attr, attr;
	cache_attr_set *attr_set;

	/* oattr objects */
	char *s_str, *i_str, attrbuf[PATH_MAX], new_attrbuf[PATH_MAX];
	int             fd;
	struct iovec    wvec[MAX_VEC_SIZE];
	obj_data_t     *ovec[MAX_VEC_SIZE];
	int             i, wcount, err;

	while (1) {
		/*
		 * If there is no search don't do anything 
		 */
		pthread_mutex_lock(&shared_mutex);
		while (search_active == 0) {
			pthread_cond_wait(&bg_active_cv, &shared_mutex);
		}
		pthread_mutex_unlock(&shared_mutex);

		/*
		 * get the next lookup object 
		 */
		tobj = ocache_queue_pop();
		if (tobj->type != INSERT_START) {
			if (tobj->type == INSERT_OATTR)
				odisk_release_obj(tobj->u.oattr.obj);
			free(tobj);
			continue;
		}

		/*
		 * for one thread case, we could do it in this simple way.
		 * XXX: do we need to change this later?
		 */
		correct = 0;
		cache_table = NULL;
		fd = -1;
		attrbuf[0] = '\0';

		/* ocache */
		cobj = (cache_obj *) calloc(1, sizeof(*cobj));
		assert(cobj != NULL);
		memcpy(&cobj->id_sig, &tobj->id_sig, sizeof(sig_val_t));
		cobj->aeval_count = 1;
		cobj->ahit_count = 1;
		if (if_cache_table)
			cache_table = tobj->u.start.cache_table;

		/* oattr */
		s_str = sig_string(&tobj->u.start.fsig);
		i_str = sig_string(&tobj->id_sig);
		assert(s_str != NULL && i_str != NULL);

		if (if_cache_oattr) {
			sprintf(attrbuf, "%s/%s/%s", cstate->ocache_path,
				s_str, i_str);
			fd = open(attrbuf, O_WRONLY | O_CREAT | O_EXCL, 0777);
			if (fd == -1) {
				if (errno != EEXIST)
					printf("failed to open %s \n", attrbuf);
			}
			else if (flock(fd, LOCK_EX) != 0) {
				perror("error locking oattr file\n");
				close(fd);
				fd = -1;
			}
		}

		free(s_str);
		free(i_str);

		wcount = 0;
		free(tobj);

		while (1) {
			tobj = ocache_queue_pop();

			if (!sig_match(&cobj->id_sig, &tobj->id_sig) ||
			    tobj->type == INSERT_START)
			{
				if (tobj->type == INSERT_OATTR)
					odisk_release_obj(tobj->u.oattr.obj);
				free(tobj);
				break;
			}

			if (tobj->type == INSERT_END) {
				cobj->result = tobj->u.end.result;
				cobj->qid = tobj->u.end.qid;
				cobj->exec_mode = tobj->u.end.exec_mode;
				correct = 1;
				free(tobj);
				break;
			}

			/* tobj->type == INSERT_IATTR or INSERT_OATTR */

			if (tobj->type == INSERT_OATTR) {
				attr_record_t *arec;

				memset(&attr, 0, sizeof(attr));
				attr.name_len = tobj->u.oattr.name_len;
				strncpy(attr.the_attr_name, tobj->u.oattr.name,
					MAX_ATTR_NAME);
				odisk_get_attr_sig(tobj->u.oattr.obj,
						   tobj->u.oattr.name,
						   &attr.attr_sig);

				arec = odisk_get_arec(tobj->u.oattr.obj,
						      tobj->u.oattr.name);
				if (arec) {
					wvec[wcount].iov_base = arec;
					wvec[wcount].iov_len = arec->rec_len;
					ovec[wcount] = tobj->u.oattr.obj;
					wcount++;
				} else
					odisk_release_obj(tobj->u.oattr.obj);

				src_attr = &attr;
				attr_set = &cobj->oattr;
			} else {
				src_attr = &tobj->u.iattr;
				attr_set = &cobj->iattr;
			}

			free(tobj);

			if ((attr_set->entry_num % ATTR_ENTRY_NUM) == 0) {
				tmp = calloc(attr_set->entry_num +
					     ATTR_ENTRY_NUM,
					     sizeof(cache_attr_entry *));
				assert(tmp != NULL);
				if (attr_set->entry_data != NULL) {
					memcpy(tmp, attr_set->entry_data,
					       attr_set->entry_num *
					       sizeof(cache_attr_entry *));
					free(attr_set->entry_data);
				}
				attr_set->entry_data = tmp;
			}

			tmp = &attr_set->entry_data[attr_set->entry_num];
			*tmp = calloc(1, sizeof(cache_attr_entry));
			assert(*tmp != NULL);
			memcpy(*tmp, src_attr, sizeof(cache_attr_entry));
			attr_set->entry_num++;

			/* if the oattr write vector is full then flush it */
			if (wcount == MAX_VEC_SIZE) {
				if (fd != -1) {
					err = writev(fd, wvec, wcount);
					assert(err >= 0);
				}
				for (i = 0; i < wcount; i++)
					odisk_release_obj(ovec[i]);
				wcount = 0;
			}
		}

		/* flush and release remaining oattr cache entries */
		if (fd != -1) {
			err = writev(fd, wvec, wcount);
			assert(err >= 0);
			close(fd);
			fd = -1;
		}
		for (i = 0; i < wcount; i++)
			odisk_release_obj(ovec[i]);

		sig_iattr(&cobj->iattr, &cobj->iattr_sig);

		if (fd != -1) {
			if (correct) {
				s_str = sig_string(&cobj->iattr_sig);
				assert(s_str != NULL);
				sprintf(new_attrbuf, "%s.%s", attrbuf, s_str);
				free(s_str);
				rename(attrbuf, new_attrbuf);
			} else
				unlink(attrbuf);
		}

		if (cache_table && correct) {
			unsigned int index;
			index = sig_hash(&cobj->id_sig) % CACHE_ENTRY_NUM;
			pthread_mutex_lock(&shared_mutex);
			cobj->next = cache_table[index];
			cache_table[index] = cobj;
			cache_entry_num++;
			pthread_mutex_unlock(&shared_mutex);
		} else
			ocache_entry_free(cobj);

		if (cache_entry_num >= MAX_ENTRY_NUM)
			free_fcache_entry(cstate->ocache_path);
	}
}


int
ocache_init(char *dirp)
{
	ocache_state_t *new_state;
	int             err;
	char           *dir_path;
	int             i;

	if (dirp == NULL) {
		dir_path = dconf_get_cachedir();
	} else {
		dir_path = strdup(dirp);
	}
	if (strlen(dir_path) > (MAX_DIR_PATH - 1)) {
		return (EINVAL);
	}
	err = mkdir(dir_path, 0777);
	if (err && errno != EEXIST) {
		printf("fail to creat cache dir (%s), err %d\n", dir_path,
		       errno);
		free(dir_path);
		return (EPERM);
	}

	if (!g_thread_supported()) g_thread_init(NULL);

	/*
	 * dctl control 
	 */
	dctl_register_leaf(DEV_CACHE_PATH, "cache_table", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &if_cache_table);
	dctl_register_leaf(DEV_CACHE_PATH, "cache_oattr", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &if_cache_oattr);

	ocache_queue = g_async_queue_new();

	new_state = (ocache_state_t *) calloc(1, sizeof(*new_state));
	assert(new_state != NULL);

	/*
	 * set callback functions so we get notifice on read/and writes
	 * to object attributes.
	 */
	lf_set_read_cb(ocache_add_iattr);
	lf_set_write_cb(ocache_add_oattr);

	/*
	 * the length has already been tested above 
	 */
	strcpy(new_state->ocache_path, dir_path);

	/*
	 * initialized the cache_table 
	 */
	for (i = 0; i < FCACHE_NUM; i++) {
		filter_cache_table[i] = NULL;
	}

	ocache_init_read(dir_path);
	/*
	 * create thread to process inserted entries for cache table 
	 */
	err = pthread_create(&new_state->c_thread_id, NULL,
			     ocache_main, (void *) new_state);

	free(dir_path);
	return (0);
}

int
ocache_start()
{
	pthread_mutex_lock(&shared_mutex);
	search_active = 1;
	search_done = 0;
	pthread_cond_signal(&bg_active_cv);
	pthread_mutex_unlock(&shared_mutex);
	return (0);
}

/*
 * called by search_close_conn in adiskd
 */
int
ocache_stop(char *dirp)
{
	int             i;
	char           *dir_path;

	if (dirp == NULL) {
		dir_path = dconf_get_cachedir();
	} else {
		dir_path = strdup(dirp);
	}

	pthread_mutex_lock(&shared_mutex);
	search_active = 0;
	search_done = 1;
	pthread_mutex_unlock(&shared_mutex);

	for (i = 0; i < FCACHE_NUM; i++) {
		if (filter_cache_table[i] == NULL)
			continue;
		ocache_write_file(dir_path, filter_cache_table[i]);
		free(filter_cache_table[i]->cache_table);
		free(filter_cache_table[i]);
		filter_cache_table[i] = NULL;
	}

	ocache_init_write(dir_path);
	free(dir_path);
	return (0);
}

/*
 * called by ceval_stop, ceval_stop is called when Stop 
 */
int
ocache_stop_search(sig_val_t * fsig)
{
	int             i;

	for (i = 0; i < FCACHE_NUM; i++) {
		if (filter_cache_table[i] == NULL) {
			continue;
		}
		if (sig_match(&filter_cache_table[i]->fsig, fsig)) {
			filter_cache_table[i]->running--;
		}
	}
	return (0);
}
