/*
 * 	Diamond (Release 1.0)
 *      A system for interactive brute-force search
 *
 *      Copyright (c) 2002-2005, Intel Corporation
 *      All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _OBJ_ATTR_H_ 
#define _OBJ_ATTR_H_	1

#include <stdint.h>


struct obj_data;
struct odisk_state;

typedef struct obj_adata {
	size_t           	adata_len;
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
	sig_val_t			attr_sig;
	int				flags;
	char 				data[0];
} attr_record_t;



#define	ATTR_FLAG_FREE		0x01
#define	ATTR_FLAG_RECOMPUTE	0x02

/* constant for the extend increment size */
#define	ATTR_INCREMENT	4096
#define	ATTR_MIN_FRAG	(sizeof(attr_record_t) + 64)

#define	ATTR_BIG_THRESH	1000



/*
 * These are the object attribute managment calls.
 */
int obj_write_attr(obj_attr_t *attr, const char *name,
                   size_t len, const char *data);
int obj_read_attr(obj_attr_t *attr, const char *name,
                  size_t *len, void *data);

int obj_ref_attr(obj_attr_t *attr, const char * name, size_t *len, void **data);

int obj_del_attr(obj_attr_t *attr, const char *name);
int obj_read_attr_file(struct odisk_state *odisk, char *attr_fname, 
		obj_attr_t *attr);
int obj_write_attr_file(char *attr_fname, obj_attr_t *attr);

int obj_get_attr_first(obj_attr_t *attr, char **buf, size_t *len,
                       void **cookie, int skip_big);

int obj_get_attr_next(obj_attr_t *attr, char **buf, size_t *len,
                      void **cookie, int skip_big);

int obj_read_oattr(struct odisk_state *odisk, char *disk_path, 
	sig_val_t *id_sig, sig_val_t *fsig, sig_val_t *iattrsig, 
	obj_attr_t *attr);

attr_record_t * odisk_get_arec(struct obj_data *obj, const char *name);


#endif                          /* ! _OBJ_ATTR_H_ */
