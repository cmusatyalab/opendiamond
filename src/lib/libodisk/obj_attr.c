/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include "lib_od.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"



int
obj_read_attr_file(char *attr_fname, obj_attr_t *attr)
{
	int		attr_fd;
	struct stat	stats;
	int		err;
	off_t		size;
	off_t		rsize;

	/* clear the umask so we get the permissions we want */
	/* XXX do we really want to do this ??? */
	umask(0000);

	/*
	 * Open the file or create it.
	 */
	attr_fd = open(attr_fname, O_CREAT|O_RDWR, 00777);
	if (attr_fd == -1) {
		perror("failed to open stat file");
		exit(1);
	}

	err = fstat(attr_fd, &stats);
	if (err != 0) {
		perror("failed to stat attributes\n");
		exit(1);
	}

	size = stats.st_size;

	if (size == 0) {
		attr->attr_len = 0;
		attr->attr_data = NULL;
	} else  {
		attr->attr_len = size;
		attr->attr_data = (char *)malloc(size);
		if (attr->attr_data == NULL) {
			perror("no memory available");
			exit(1);
		}
		rsize = read(attr_fd, attr->attr_data, size);
		if (rsize != size) {
			perror("failed to read all data \n");
			exit(1);
		}
	}

	close(attr_fd);
	return(0);
}



int
obj_write_attr_file(char *attr_fname, obj_attr_t *attr)
{
	int		attr_fd;
	off_t		wsize;
	size_t		len;
	char 	*	buf;
	int			err;
	void *		cookie;

	/* clear the umask so we get the permissions we want */
	/* XXX do we really want to do this ??? */
	umask(0000);

	/*
	 * Open the file or create it.
	 */
	attr_fd = open(attr_fname, O_CREAT|O_WRONLY|O_TRUNC, 00777);
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
	return(0);
}



static int
extend_attr_store(obj_attr_t *attr, int new_size)
{

	char *			new_attr;
	int			new_len;
	int			offset;
	attr_record_t * 	new_record;
	int			real_increment;


	/* we extend the store by multiples of attr_increment */ 
	real_increment = (new_size + (ATTR_INCREMENT -1)) &
		~(ATTR_INCREMENT - 1);

	new_len = attr->attr_len + real_increment;

	new_attr = (char *)malloc(new_len);
	if (new_attr == NULL) {
		/* XXX log */
		return(ENOMEM);
	}

	memcpy(new_attr, attr->attr_data, attr->attr_len);
	free(attr->attr_data);

	attr->attr_data = new_attr;
	attr->attr_len = new_len;

	/*
	 * We need to make the new data a record. Ideally
	 * we will coalesce with the data before this but 
	 * this will be later.  
	 * XXX coalsce.
	 */
	offset = new_len - real_increment;
	new_record = (attr_record_t *) &new_attr[offset];
	new_record->rec_len = real_increment;
	new_record->flags = ATTR_FLAG_FREE;

	return (0);
}

/*
 * This finds a free record of the specified size.
 * This may be done by splitting another record, etc.
 *
 * If we can't find a record the necessary size, then we try
 * to extend the space used by the attributes.
 */
static attr_record_t *
find_free_record(obj_attr_t *attr, int size)
{
	attr_record_t *		cur_rec;
	attr_record_t *		new_frag;
	int			cur_offset;
	int			err;

    printf("find free record \n\n");

	cur_offset = 0;
	while (1) {
		printf("cur_off %d len %d \n", cur_offset, attr->attr_len);
		assert(cur_offset <= attr->attr_len);
		if (cur_offset == attr->attr_len) {
			printf("extending attr store: olen %d off %d size %d \n",
				attr->attr_len, cur_offset, size);
			err = extend_attr_store(attr, size);
			if (err == ENOMEM) {
				return (NULL);
			}
			printf("after extend store: alen %d off %d size %d \n",
				attr->attr_len, cur_offset, size);
		}

		cur_rec = (attr_record_t *)&attr->attr_data[cur_offset];
		if (((cur_rec->flags & ATTR_FLAG_FREE) == ATTR_FLAG_FREE) &&
		     (cur_rec->rec_len >= size)) {
			break;
		}
		/* this one doesn't work advance */
		cur_offset += cur_rec->rec_len;
	}


	/* we have chunk, now decide if we want to split it */
	if ((cur_rec->rec_len - size) >= ATTR_MIN_FRAG) {
		new_frag = (attr_record_t *)&attr->attr_data[cur_offset+size];
		new_frag->rec_len = cur_rec->rec_len - size;
		new_frag->flags = ATTR_FLAG_FREE;

		/* set the new size and clear free flag */
		cur_rec->rec_len = size;
		cur_rec->flags &= ~ATTR_FLAG_FREE;
	} else {
		/* the only thing we do is clear free flag */
		cur_rec->flags &= ~ATTR_FLAG_FREE;
	}

	return(cur_rec);
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
free_record(obj_attr_t *attr, attr_record_t *rec)
{

	/*
	 * XXX try to colesce in later versions ???
	 */
	rec->flags |= ATTR_FLAG_FREE;
}


/*
 * Look for a record that matches a specific name string.  If
 * we find a match, we return a pointer to this record, otherwise
 * we return NULL.
 */


static attr_record_t *
find_record(obj_attr_t *attr, const char *name)
{
	int			namelen;
	int			cur_offset;
	attr_record_t *		cur_rec = NULL;

	namelen = strlen(name) + 1;	/* include termination */
	cur_offset = 0;
	while (cur_offset < attr->attr_len) {
		cur_rec = (attr_record_t *)&attr->attr_data[cur_offset];
		if (((cur_rec->flags & ATTR_FLAG_FREE) == 0) &&
		     (cur_rec->name_len >= namelen) &&
		     (strcmp(name, cur_rec->data) == 0)) {
			break;
		}

		/* this one doesn't work advance */
		cur_offset += cur_rec->rec_len;

	}
	if (cur_offset >= attr->attr_len) {
		cur_rec = NULL;
	}
	return(cur_rec);
}



/*
 * This writes an attribute related with with an object.
 */

int 
obj_write_attr(obj_attr_t *attr, const char * name, off_t len, const char *data)
{
	attr_record_t *	data_rec; 
	int		total_size;
	int		namelen;

	/* XXX validate object ??? */
	/* XXX make sure we don't have the same name on the list ?? */

	namelen = strlen(name) + 1;

	/* XXX this overcounts data space !! \n */
	total_size  = sizeof(*data_rec) + namelen + len;


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
			/* XXX log */
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

	return(0);
}

/*
 *  This reads a piece of named ancillary data.  
 */

int 
obj_read_attr(obj_attr_t *attr, const char * name, off_t *len, char *data)
{
	attr_record_t *		record;
	char *			dptr;


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
	/* looks good, return the data */
	memcpy(data, dptr, record->data_len);
	return(0);
}


/*
 * Delete an attribute that was previously associated
 * with the object.
 */
	
int 
obj_del_attr(obj_attr_t *attr, const char * name)
{
	attr_record_t *		record;

	record = find_record(attr, name);
	if (record == NULL) {
		return (ENOENT);
	}

	free_record(attr, record);
	return(0);
}

int 
obj_get_attr_first(obj_attr_t *attr, char **buf, size_t *len, void **cookie,
	int  skip_big)
{
	attr_record_t *		record;
	size_t			offset;

	offset = 0;

again:
	/* see if we have gone through all the attributes */
	if (offset >= attr->attr_len) {
		printf("first: no data \n");
		return(ENOENT);
	}

	record = (attr_record_t *)&attr->attr_data[offset];
	offset += record->rec_len;

	/* Skip any records without data */
	if (record->flags & ATTR_FLAG_FREE) {
		goto again;
	}

	/* XXX see if we should toss this */
	if ((record->data_len > ATTR_BIG_THRESH) && (skip_big)) {
		goto again;
	}

	//*len = record->name_len + record->data_len + sizeof(struct attr_record);
	*len = record->rec_len;
	*buf = (void *)record;

	*cookie = (void *)offset;

	return(0);
}

int 
obj_get_attr_next(obj_attr_t *attr, char **buf, size_t *len, void **cookie,
	int skip_big)
{
	attr_record_t *		record;
	size_t			offset;

	offset = (size_t)*cookie;

again:
	/* see if we have gone through all the attributes */
	if (offset >= attr->attr_len) {
		return(ENOENT);
	}

	record = (attr_record_t *)&attr->attr_data[offset];
	offset += record->rec_len;

	/* Skip any records without data */
	if (record->flags & ATTR_FLAG_FREE) {
		goto again;
	}

	if ((record->data_len > ATTR_BIG_THRESH) && (skip_big)) {
		goto again;
	}

	/* XXX see if we should toss this */
	*len = record->name_len + record->data_len + sizeof(struct attr_record);
	*buf = (void *)record;

	*cookie = (void *)offset;

	return(0);
}
