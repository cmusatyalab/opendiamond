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

#ifndef _OBJ_ATTR_H_
#define _OBJ_ATTR_H_	1

#include <stdint.h>


struct obj_data;

typedef struct obj_adata {
    off_t           	adata_len;
    char           	*	adata_data;
    char           	*	adata_base;
    struct obj_adata *	adata_next;
} obj_adata_t;


/*
 * This is the state associated with the object
 */
typedef struct {
	int				attr_ndata;
	obj_adata_t *	attr_dlist;	
} obj_attr_t;


/*
 * XXX we need to store these in network byte order and fixed
 * size for sharing across the network.
 */


/* maximum signature size we support */
#define	ATTR_MAX_SIG		20

typedef struct attr_record {
	int				rec_len;
	int				name_len;
	int				data_len;
	unsigned char	attr_sig[ATTR_MAX_SIG];
	int				flags;
	char 			data[0];
} attr_record_t;



#define	ATTR_FLAG_FREE		0x01
#define	ATTR_FLAG_RECOMPUTE	0x02

/* constant for the extend increment size */
#define	ATTR_INCREMENT	4096
#define	ATTR_MIN_FRAG	(sizeof(attr_record_t) + 64)

#define	ATTR_BIG_THRESH	1000


/*
 * The extension on a file that shows it is an attribute.
 */
#define	ATTR_EXT	".attr"
/*
 * The maximum lenght of the name string for an attribute.
 */
#define	MAX_ATTR_NAME	1024


/*
 * These are the object attribute managment calls.
 */
int obj_write_attr(obj_attr_t *attr, const char *name,
			  off_t len, const char *data);
int obj_read_attr(obj_attr_t *attr, const char *name,
			 off_t *len, char *data);

int obj_ref_attr(obj_attr_t *attr, const char * name, off_t *len, char **data);

int obj_del_attr(obj_attr_t *attr, const char *name);
int obj_read_attr_file(char *attr_fname, obj_attr_t *attr);
int obj_write_attr_file(char *attr_fname, obj_attr_t *attr);

int obj_get_attr_first(obj_attr_t *attr, char **buf, size_t *len, 
	void **cookie, int skip_big);

int obj_get_attr_next(obj_attr_t *attr, char **buf, size_t *len, 
	void **cookie, int skip_big);

//int obj_read_oattr(char *disk_path, char *fname, obj_attr_t *attr);
int obj_read_oattr(char *disk_path, uint64_t oid, char *fsig, char *iattrsig, obj_attr_t *attr);

attr_record_t * odisk_get_arec(struct obj_data *obj, const char *name);


#endif                          /* ! _OBJ_ATTR_H_ */
