/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
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
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/file.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_dconfig.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "lib_log.h"
#include "odisk_priv.h"
#include "sig_calc_priv.h"

static int obj_get_attr_first(obj_attr_t *attr, unsigned char **buf,
			      size_t *len, struct acookie **cookie,
			      int skip_big);
static int obj_get_attr_next(obj_attr_t *attr, unsigned char **buf,
			     size_t *len, struct acookie **cookie,
			     int skip_big);
int
obj_read_attr_file(odisk_state_t * odisk, char *attr_fname, obj_attr_t * attr)
{
	int             attr_fd;
	struct stat     stats;
	int             err;
	size_t          size;
	size_t          rsize;
	obj_adata_t    *adata;

	/*
	 * Open the file or create it.
	 */
	attr_fd = open(attr_fname, odisk->open_flags, 00777);
	if (attr_fd == -1) {
		attr->attr_ndata = 0;
		attr->attr_dlist = NULL;
		return (0);
	}

	err = fstat(attr_fd, &stats);
	if (err != 0) {
		attr->attr_ndata = 0;
		attr->attr_dlist = NULL;
		return (0);
	}

	size = stats.st_size;

	if (size == 0) {
		attr->attr_ndata = 0;
		attr->attr_dlist = NULL;
	} else {
		adata = (obj_adata_t *) malloc(sizeof(*adata));
		assert(adata != NULL);

		adata->adata_len = size;
		adata->adata_data = (char *) malloc(size);
		assert(adata->adata_data != NULL);

		attr->attr_ndata = 1;
		adata->adata_next = NULL;
		attr->attr_dlist = adata;

		rsize = read(attr_fd, adata->adata_data, size);
		if (rsize != size) {
			perror("Reading attribute file:");
			free(adata->adata_data);
			attr->attr_ndata = 0;
			attr->attr_dlist = NULL;
			close(attr_fd);
			return (0);
		}
	}

	close(attr_fd);
	return (0);
}

int
obj_write_attr_file(char *attr_fname, obj_attr_t * attr)
{
	int		attr_fd;
	off_t		wsize;
	size_t		len;
	unsigned char *	buf;
	int		err;
	struct acookie  *cookie;

	/*
	 * Open the file or create it.
	 */
	attr_fd = open(attr_fname, O_CREAT | O_WRONLY | O_TRUNC, 00700);
	if (attr_fd == -1) {
		perror("failed to open stat file");
		exit(1);
	}

	err = obj_get_attr_first(attr, &buf, &len, &cookie, 0);
	while (err != ENOENT) {
		wsize = write(attr_fd, buf, len);
		if (wsize != len) {
			perror("failed to write attributes \n");
			exit(1);
		}
		err = obj_get_attr_next(attr, &buf, &len, &cookie, 0);
	}

	close(attr_fd);
	return (0);
}



static char    *
extend_attr_store(obj_attr_t * attr, int new_size)
{
	char           *new_attr;
	attr_record_t  *new_record;
	int             buf_size;
	obj_adata_t    *new_adata;

	/*
	 * we extend the store by multiples of attr_increment 
	 */
	buf_size = (new_size + (ATTR_INCREMENT - 1)) & ~(ATTR_INCREMENT - 1);
	assert(buf_size >= new_size);

	new_attr = (char *) malloc(buf_size);
	assert(new_attr != NULL);

	new_record = (attr_record_t *) new_attr;
	new_record->rec_len = buf_size;
	new_record->flags = ATTR_FLAG_FREE;

	new_adata = (obj_adata_t *) malloc(sizeof(*new_adata));
	assert(new_adata != NULL);
	new_adata->adata_len = buf_size;
	new_adata->adata_data = new_attr;

	/*
	 * link it on the list of items 
	 */
	new_adata->adata_next = attr->attr_dlist;
	attr->attr_dlist = new_adata;
	attr->attr_ndata++;

	return (new_attr);
}

/*
 * This finds a free record of the specified size.
 * This may be done by splitting another record, etc.
 *
 * If we can't find a record the necessary size, then we try
 * to extend the space used by the attributes.
 */
static attr_record_t *
find_free_record(obj_attr_t * attr, int size)
{
	obj_adata_t    *cur_adata;
	attr_record_t  *cur_rec;
	attr_record_t  *new_frag;
	attr_record_t  *good_rec = NULL;
	int             offset;

	if (size < 0) {
		return (NULL);
	}

	for (cur_adata = attr->attr_dlist; cur_adata != NULL;
	     cur_adata = cur_adata->adata_next) {

		offset = 0;
		while (offset < cur_adata->adata_len) {
			cur_rec =
			    (attr_record_t *) & cur_adata->adata_data[offset];
			if (((cur_rec->flags & ATTR_FLAG_FREE) ==
			     ATTR_FLAG_FREE) && (cur_rec->rec_len >= size)) {
				good_rec = cur_rec;
				goto match;
			}
			offset += cur_rec->rec_len;
		}
	}


	/*
	 * if we got here, we fell through, create some more space and use
	 * it. 
	 */

	good_rec = (attr_record_t *) extend_attr_store(attr, size);

      match:

	/*
	 * we have record, we if we want to split it 
	 */
	if ((good_rec->rec_len - size) >= ATTR_MIN_FRAG) {
		new_frag = (attr_record_t *) & (((char *) good_rec)[size]);
		new_frag->rec_len = good_rec->rec_len - size;
		new_frag->flags = ATTR_FLAG_FREE;
		/*
		 * set the new size and clear free flag 
		 */
		good_rec->rec_len = size;
		good_rec->flags &= ~ATTR_FLAG_FREE;
	} else {
		/*
		 * the only thing we do is clear free flag 
		 */
		good_rec->flags &= ~ATTR_FLAG_FREE;
	}
	return (good_rec);
}

/*
 * Mark a given attribute record as free.  This is done
 * by setting the free flag.  
 *
 * XXX in the future we should look for items before, after
 * this that also have free space to see if we can collapse them.
 *
 */

static void
free_record(obj_attr_t * attr, attr_record_t * rec)
{

	/*
	 * XXX try to colesce in later versions ???
	 */
	rec->flags = ATTR_FLAG_FREE;
}


/*
 * Look for a record that matches a specific name string.  If
 * we find a match, we return a pointer to this record, otherwise
 * we return NULL.
 */


static attr_record_t *
find_record(obj_attr_t * attr, const char *name)
{
	int             namelen;
	int             offset;
	obj_adata_t    *cur_adata;
	attr_record_t  *cur_rec;

	if (name == NULL) {
		return (NULL);
	}
	namelen = strlen(name) + 1;	/* include termination */


	for (cur_adata = attr->attr_dlist; cur_adata != NULL;
	     cur_adata = cur_adata->adata_next) {

		offset = 0;
		while (offset < cur_adata->adata_len) {
			cur_rec =
			    (attr_record_t *) & cur_adata->adata_data[offset];

			if (((cur_rec->flags & ATTR_FLAG_FREE) == 0) &&
			    (cur_rec->name_len >= namelen) &&
			    (strcmp(name, (char *)cur_rec->data) == 0)) {
				return (cur_rec);
			}
			offset += cur_rec->rec_len;
		}
	}

	return (NULL);
}

int
odisk_get_attr_sig(obj_data_t * obj, const char *name, sig_val_t * sig)
{
	attr_record_t  *arec;

	arec = find_record(&obj->attr_info, name);
	if (arec == NULL) {
		return (ENOENT);
	}
	memcpy(sig, &arec->attr_sig, sizeof(sig_val_t));
	return (0);
}


/*
 * This writes an attribute related with with an object.
 */

int
obj_write_attr(obj_attr_t * attr, const char *name, size_t len,
	       const unsigned char *data)
{
	attr_record_t  *data_rec;
	int             total_size;
	int             namelen;

	/*
	 * XXX validate object ??? 
	 */
	/*
	 * XXX make sure we don't have the same name on the list ?? 
	 */

	if (name == NULL) {
		return (EINVAL);
	}
	namelen = strlen(name) + 1;

	if (namelen > MAX_ATTR_NAME) {
		return (EINVAL);
	}

	/*
	 * XXX round to word boundary ?? 
	 */
	total_size = sizeof(*data_rec) + namelen + len;

	data_rec = find_record(attr, name);
	if (data_rec != NULL) {
		/*
		 * If we have an existing record make sure it is large
		 * enough to hold the new data.
		 */
		if (data_rec->rec_len < total_size) {
			free_record(attr, data_rec);
			data_rec = NULL;
		}
	}

	if (data_rec == NULL) {
		data_rec = find_free_record(attr, total_size);
		if (data_rec == NULL) {
			log_message(LOGT_DISK, LOGL_ERR, 
			   "write_attr: failed allocate space");
			return (ENOMEM);
		}
	}

	/*
	 * Now we have the record, so write in the data.
	 */
	data_rec->name_len = namelen;
	data_rec->data_len = len;
	memcpy(data_rec->data, name, namelen);
	memcpy(&data_rec->data[namelen], data, len);

	/*
	 * compute the attribute signature 
	 */
	sig_cal(data, len, &data_rec->attr_sig);

	return (0);
}

/*
 *  This reads a piece of named ancillary data.  
 */

int
obj_read_attr(obj_attr_t * attr, const char *name, size_t * len, 
		unsigned char *data)
{
	attr_record_t  *record;
	unsigned char         *dptr;

	if ((strlen(name) + 1) > MAX_ATTR_NAME) {
		return (EINVAL);
	}

	record = find_record(attr, name);
	if (record == NULL) {
		return (ENOENT);
	}



	/*
	 * We set the return length to the amount of data and
	 * make sure data is large enought to hold it.  We return
	 * an error if it isnt' big enough.  It is important to set the
	 * value first because we need to set in both the error and
	 * non-error cases.
	 */
	if (record->data_len > *len) {
		*len = record->data_len;
		return (ENOMEM);
	}
	*len = record->data_len;

	/*
	 * set dptr to the data portion of the record,
	 * and copy the data in.
	 */
	dptr = &record->data[record->name_len];
	/*
	 * looks good, return the data 
	 */
	memcpy(data, dptr, record->data_len);
	return (0);
}

/*
 * this gets a reference to the attributes and returns a pointer to
 * the real data.  The caller may not modify it.
 */
int
obj_ref_attr(obj_attr_t * attr, const char *name, size_t * len, 
		unsigned char **data)
{
	attr_record_t  *record;

	record = find_record(attr, name);
	if (record == NULL) {
		return (ENOENT);
	}

	*len = record->data_len;
	*data = &record->data[record->name_len];

	return (0);
}

/*
 * Mark this attribute as "omitted". Omitted attributes
 * are never sent upstream.
 */
int
obj_omit_attr(obj_attr_t * attr, const char *name)
{
	attr_record_t  *record;

	record = find_record(attr, name);
	if (record == NULL) {
		return (ENOENT);
	}

	record->flags |= ATTR_FLAG_OMIT;
	return (0);
}


/*
 * Delete an attribute that was previously associated
 * with the object.
 */

int
obj_del_attr(obj_attr_t * attr, const char *name)
{
	attr_record_t  *record;

	record = find_record(attr, name);
	if (record == NULL) {
		return (ENOENT);
	}

	free_record(attr, record);
	return (0);
}


static int
obj_use_record(attr_record_t * cur_rec, int skip_big)
{

	if (cur_rec->flags & ATTR_FLAG_FREE) {
		return (0);
	}

	if (cur_rec->flags & ATTR_FLAG_OMIT) {
		return (0);
	}

	if ((cur_rec->data_len > ATTR_BIG_THRESH) && (skip_big)) {
		return (0);
	}
	return (1);
}

struct acookie {
	size_t		offset;
	obj_adata_t	*adata;
};

static int
obj_get_attr_first(obj_attr_t *attr, unsigned char **buf, size_t *len,
		   struct acookie **cookie, int skip_big)
{
	size_t		offset;
	obj_adata_t    *cur_adata;
	attr_record_t  *cur_rec;

	for (cur_adata = attr->attr_dlist; cur_adata != NULL;
	     cur_adata = cur_adata->adata_next)
	{
		offset = 0;
		while (offset < cur_adata->adata_len) {
			cur_rec =
			    (attr_record_t *)&cur_adata->adata_data[offset];

			offset += cur_rec->rec_len;
			if (!obj_use_record(cur_rec, skip_big))
				continue;

			*len = cur_rec->rec_len;
			*buf = (void *) cur_rec;

			*cookie = malloc(sizeof(**cookie));
			assert(*cookie != NULL);

			(*cookie)->offset = offset;
			(*cookie)->adata = cur_adata;
			return 0;
		}
	}
	*cookie = NULL;
	return ENOENT;
}

int
obj_first_attr(obj_attr_t *attr, char **name, size_t *len,
	       unsigned char **data, sig_val_t **sig,
	       struct acookie **cookie, int skip_big)
{
	attr_record_t	*cur_rec;
	unsigned char	*dbuf;
	size_t		rec_len;
	int		err;

	err = obj_get_attr_first(attr, &dbuf, &rec_len, cookie, skip_big);
	if (err) return err;

	cur_rec = (attr_record_t *)dbuf;

	if (name) *name = (char *)&cur_rec->data[0];
	if (data) *data = &cur_rec->data[cur_rec->name_len];
	if (len) *len = cur_rec->data_len;
	if (sig) *sig = &cur_rec->attr_sig;
	return 0;
}

static int
obj_get_attr_next(obj_attr_t *attr, unsigned char **buf, size_t *len,
		  struct acookie **cookie, int skip_big)
{
	size_t		offset;
	obj_adata_t    *cur_adata;
	attr_record_t  *cur_rec;

	if (!*cookie) return ENOENT;

	offset = (*cookie)->offset;
	for (cur_adata = (*cookie)->adata; cur_adata != NULL;
	     cur_adata = cur_adata->adata_next)
	{
		while (offset < cur_adata->adata_len) {
			cur_rec =
			    (attr_record_t *)&cur_adata->adata_data[offset];

			offset += cur_rec->rec_len;
			if (!obj_use_record(cur_rec, skip_big))
				continue;

			*len = cur_rec->rec_len;
			*buf = (void *) cur_rec;

			(*cookie)->offset = offset;
			(*cookie)->adata = cur_adata;
			return 0;
		}
		offset = 0;
	}
	free(*cookie);
	*cookie = NULL;
	return ENOENT;
}


int
obj_next_attr(obj_attr_t *attr, char **name, size_t *len,
	      unsigned char **data, sig_val_t **sig,
	      struct acookie **cookie, int skip_big)
{
	unsigned char * dbuf;
	attr_record_t  *cur_rec;
	size_t		rec_len;
	int		err;

	err = obj_get_attr_next(attr, &dbuf, &rec_len, cookie, skip_big);
	if (err) return err;

	cur_rec = (attr_record_t *)dbuf;

	if (name) *name = (char *)&cur_rec->data[0];
	if (data) *data = &cur_rec->data[cur_rec->name_len];
	if (len) *len = cur_rec->data_len;
	if (sig) *sig = &cur_rec->attr_sig;
	return 0;
}

attr_record_t  *
odisk_get_arec(struct obj_data * obj, const char *name)
{
	attr_record_t  *arec;

	arec = find_record(&obj->attr_info, name);
	return (arec);
}
