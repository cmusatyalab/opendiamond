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
#include <sys/file.h>
#include "lib_od.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "sig_calc.h"

#define CACHE_DIR               "cache"

int
obj_read_attr_file(char *attr_fname, obj_attr_t *attr)
{
	int		attr_fd;
	struct stat	stats;
	int		err;
	off_t		size;
	off_t		rsize;
	obj_adata_t *	adata;

	/* clear the umask so we get the permissions we want */
	/* XXX do we really want to do this ??? */
	umask(0000);

	/*
	 * Open the file or create it.
	 */
	attr_fd = open(attr_fname, O_CREAT|O_RDONLY|O_DIRECT, 00777);
	if (attr_fd == -1) {
		perror("failed to open attr file");
		exit(1);
	}

	err = fstat(attr_fd, &stats);
	if (err != 0) {
		perror("failed to stat attributes\n");
		exit(1);
	}

	size = stats.st_size;

	if (size == 0) {
		attr->attr_ndata = 0;
		attr->attr_dlist = NULL;
	} else  {
		adata = (obj_adata_t *)malloc(sizeof(*adata));
		assert(adata != NULL);

		adata->adata_len = size;
		adata->adata_base = (char *)malloc(ALIGN_SIZE(size));
		assert(adata->adata_base != NULL);
		adata->adata_data = (char *)ALIGN_VAL(adata->adata_base);

		attr->attr_ndata = 1;
		adata->adata_next = NULL;
		attr->attr_dlist = adata;

		rsize = read(attr_fd, adata->adata_data, ALIGN_ROUND(size));
		if (rsize != size) {
			printf("failed to read all data: got %ld wanted %ld algin %ld\n",
				rsize, size, ALIGN_ROUND(size));
			exit(1);
		}
	}

	close(attr_fd);
	return(0);
}

int
obj_write_attr_file(char *attr_fname, obj_attr_t *attr)
{
	int			attr_fd;
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



static char *
extend_attr_store(obj_attr_t *attr, int new_size)
{
	char *			new_attr;
	attr_record_t * 	new_record;
	int				buf_size;
	obj_adata_t *	new_adata;

	/* we extend the store by multiples of attr_increment */
	buf_size = (new_size + (ATTR_INCREMENT -1)) &
	                 ~(ATTR_INCREMENT - 1);
	assert(buf_size >= new_size);

	new_attr = (char *)malloc(buf_size);
	assert(new_attr != NULL);

	new_record = (attr_record_t *) new_attr;
	new_record->rec_len = buf_size;
	new_record->flags = ATTR_FLAG_FREE;

	new_adata = (obj_adata_t *)malloc(sizeof(*new_adata));
	assert(new_adata != NULL);
	new_adata->adata_len = buf_size;
	new_adata->adata_data = new_attr;
	new_adata->adata_base = new_attr;

	/* link it on the list of items */
	new_adata->adata_next = attr->attr_dlist;
	attr->attr_dlist = new_adata;
	attr->attr_ndata++;

	return(new_attr);
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
	obj_adata_t	*		cur_adata;
	attr_record_t *		cur_rec;
	attr_record_t *		new_frag;
	attr_record_t *		good_rec = NULL;
	int			offset;

	if (size < 0) {
		return (NULL);
	}

	for (cur_adata = attr->attr_dlist; cur_adata != NULL; 
		cur_adata = cur_adata->adata_next) {

		offset = 0;
		while (offset < cur_adata->adata_len) {
			cur_rec = (attr_record_t *)&cur_adata->adata_data[offset];
			if (((cur_rec->flags & ATTR_FLAG_FREE) == ATTR_FLAG_FREE) &&
		    	(cur_rec->rec_len >= size)) {
					good_rec = cur_rec;
					goto match;
			}
			offset += cur_rec->rec_len;
		}
	}


	/* if we got here, we fell through, create some more space and
	 * use it.
 	 */

	good_rec = (attr_record_t *)extend_attr_store(attr, size);

match:

	/* we have record, we if we want to split it */
	if ((good_rec->rec_len - size) >= ATTR_MIN_FRAG) {
		new_frag = (attr_record_t *)&(((char *)good_rec)[size]);
		new_frag->rec_len = good_rec->rec_len - size;
		new_frag->flags = ATTR_FLAG_FREE;
		/* set the new size and clear free flag */
		good_rec->rec_len = size;
		good_rec->flags &= ~ATTR_FLAG_FREE;
	} else {
		/* the only thing we do is clear free flag */
		good_rec->flags &= ~ATTR_FLAG_FREE;
	}
	return(good_rec);
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
	int			offset;
	obj_adata_t *		cur_adata;
	attr_record_t *		cur_rec;

	if (name == NULL) {
		return (NULL);
	}
	namelen = strlen(name) + 1;	/* include termination */


	for (cur_adata = attr->attr_dlist; cur_adata != NULL; 
		cur_adata = cur_adata->adata_next) {

		offset = 0;
		while (offset < cur_adata->adata_len) {
			cur_rec = (attr_record_t *)&cur_adata->adata_data[offset];

			if (((cur_rec->flags & ATTR_FLAG_FREE) == 0) &&
		    	(cur_rec->name_len >= namelen) &&
		    	(strcmp(name, cur_rec->data) == 0)) {
					return(cur_rec);
			}
			offset += cur_rec->rec_len;
		}
	}

	return(NULL);
}

int 
odisk_get_attr_sig(obj_data_t *obj, const char *name, char *data, int len)
{
	attr_record_t *	arec;


	arec = find_record(&obj->attr_info, name);
	if (arec == NULL) {
		return(ENOENT);
	}

	memcpy(data, arec->attr_sig, len);
	return(0);
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
	unsigned char *sig_ptr;

	/* XXX validate object ??? */
	/* XXX make sure we don't have the same name on the list ?? */

	if (name == NULL) {
		return (EINVAL);
	}
	namelen = strlen(name) + 1;

	/* XXX round to word boundary ?? */
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
			printf("XXXXXXXX obj_write_attr find free failed\n");
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

	/* compute the attribute signature and save it */
	memset(data_rec->attr_sig, 0, ATTR_MAX_SIG);
	sig_ptr = data_rec->attr_sig;
	sig_cal(data, len, &sig_ptr);	

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
 * this gets a reference to the attributes and returns a pointer to
 * the real data.  The caller may not modify it.
 */
int
obj_ref_attr(obj_attr_t *attr, const char * name, off_t *len, char **data) 
{
	attr_record_t *		record;

	record = find_record(attr, name);
	if (record == NULL) {
		return (ENOENT);
	}

	*len = record->data_len;
	*data = &record->data[record->name_len];

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


/* XXX short term hack */
typedef struct acookie {
	size_t		offset;
	obj_adata_t *	adata;
} acookie_t;


static int
obj_use_record(attr_record_t *cur_rec, int skip_big)
{

	if (cur_rec->flags & ATTR_FLAG_FREE) {
		return(0);
	}

	if ((cur_rec->data_len > ATTR_BIG_THRESH) && (skip_big)) {
		return(0);
	}
	return(1);	
}

int
obj_get_attr_first(obj_attr_t *attr, char **buf, size_t *len, void **cookie,
                   int  skip_big)
{
	size_t			offset;
	obj_adata_t *		cur_adata;
	attr_record_t *		cur_rec;
	acookie_t *			acookie;


	for (cur_adata = attr->attr_dlist; cur_adata != NULL; 
		cur_adata = cur_adata->adata_next) {

		offset = 0;
		while (offset < cur_adata->adata_len) {
			cur_rec = (attr_record_t *)&cur_adata->adata_data[offset];
			if (obj_use_record(cur_rec, skip_big)) {
				acookie = (acookie_t *)malloc(sizeof(*acookie));
				assert(acookie != NULL);

				acookie->offset = offset + cur_rec->rec_len;
				acookie->adata = cur_adata;
				*len = cur_rec->rec_len;
				*buf = (void *)cur_rec;
				*cookie = (void *)acookie;
				return(0);
			}
			offset += cur_rec->rec_len;
		}
	}
	return(ENOENT);
}

int
obj_get_attr_next(obj_attr_t *attr, char **buf, size_t *len, void **cookie,
                  int skip_big)
{
	size_t			offset;
	obj_adata_t *		cur_adata;
	attr_record_t *		cur_rec;
	acookie_t *			acookie;


	acookie = (acookie_t *)*cookie;

	for (cur_adata = acookie->adata; cur_adata != NULL; 
		cur_adata = cur_adata->adata_next) {

		if (cur_adata == acookie->adata) {
			offset = acookie->offset;
		} else {
			offset = 0;
		}
		while (offset < cur_adata->adata_len) {
			cur_rec = (attr_record_t *)&cur_adata->adata_data[offset];
			if (obj_use_record(cur_rec, skip_big)) {
				assert(acookie != NULL);
				acookie->offset = offset + cur_rec->rec_len;
				acookie->adata = cur_adata;
				*len = cur_rec->rec_len;
				*buf = (void *)cur_rec;
				*cookie = (void *)acookie;
				return(0);
			}
			offset += cur_rec->rec_len;
		}
	}
	free(*cookie);
	return(ENOENT);
}


int
obj_read_oattr(char *disk_path, uint64_t oid, char *fsig, char *iattrsig, 
	obj_attr_t *attr)
{
	int fd;
	off_t           rsize;
	struct stat     stats;
	char attrbuf[MAX_ATTR_NAME];
	uint64_t tmp1, tmp2, tmp3, tmp4;
	obj_adata_t *	adata;
	int err, len;

	if( (fsig == NULL) || (iattrsig == NULL) ) {
		printf("fsig or iattrsig is NULL\n");
		return (EINVAL);
	}
	memcpy(&tmp1, fsig, sizeof(tmp1) );
	memcpy(&tmp2, fsig+8, sizeof(tmp2) );
	memcpy(&tmp3, iattrsig, sizeof(tmp3) );
	memcpy(&tmp4, iattrsig+8, sizeof(tmp4) );

	len = snprintf(attrbuf, MAX_ATTR_NAME,
	               "%s/%s/%016llX%016llX/%016llX.%016llX%016llX",
	               disk_path, CACHE_DIR, tmp1, tmp2, oid, tmp3, tmp4);
	assert(len < (MAX_ATTR_NAME - 1));

	fd = open(attrbuf, (O_RDONLY|O_DIRECT));
	if (fd == -1) {
		return (EINVAL);
	}

	/* XX LH hmm, do we really nead this?  */
	//err = flock(fd, LOCK_EX);
	//if (err != 0) {
		//perror("failed to lock attributes file\n");
		//close(fd);
		//return (EINVAL);
	//}

	err = fstat(fd, &stats);
	if (err != 0) {
		perror("failed to stat attributes\n");
		close(fd);
		return (EINVAL);
	}
	if (stats.st_size == 0) {
		close(fd);
		return (0);
   } else  {
        adata = (obj_adata_t *)malloc(sizeof(*adata));
        assert(adata != NULL);
                                                                                
        adata->adata_len = stats.st_size;
        adata->adata_base = (char *)malloc(ALIGN_SIZE(stats.st_size));
        assert(adata->adata_base != NULL);
        adata->adata_data = (char *)ALIGN_VAL(adata->adata_base);
                                                                                
    
        rsize = read(fd, adata->adata_data, ALIGN_ROUND(stats.st_size));
		assert(rsize == stats.st_size);

		/* put it on the list of attrs for the object */
        attr->attr_ndata++;
        adata->adata_next = attr->attr_dlist;
        attr->attr_dlist = adata;

		close(fd);
		return(0);
    }
}


attr_record_t * 
odisk_get_arec(struct obj_data *obj, const char *name)
{
	attr_record_t *arec;

	arec = find_record(&obj->attr_info, name);
	return(arec);
}

