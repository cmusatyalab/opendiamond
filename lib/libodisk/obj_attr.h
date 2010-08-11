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

#ifndef _OBJ_ATTR_H_ 
#define _OBJ_ATTR_H_	1

#include <stdint.h>
#include "sig_calc.h"

struct obj_data;
struct odisk_state;

typedef struct obj_adata {
	size_t           	adata_len;
	char           	*	adata_data;
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
	unsigned char 			data[0];
} attr_record_t;



#define	ATTR_FLAG_FREE		0x01
#define	ATTR_FLAG_RECOMPUTE	0x02
#define	ATTR_FLAG_OMIT          0x04

/* constant for the extend increment size */
#define	ATTR_INCREMENT	4096
#define	ATTR_MIN_FRAG	(sizeof(attr_record_t) + 64)


/*
 * These are the object attribute managment calls.
 */
int obj_write_attr(obj_attr_t *attr, const char *name,
                   size_t len, const unsigned char *data);
int obj_read_attr(obj_attr_t *attr, const char *name,
                  size_t *len, unsigned char *data);


int obj_omit_attr(obj_attr_t *attr, const char *name);
int obj_del_attr(obj_attr_t *attr, const char *name);

struct acookie;
int obj_first_attr(obj_attr_t *attr, char **name, size_t *len,
		   unsigned char **data, sig_val_t **sig,
		   struct acookie **cookie);
int obj_next_attr(obj_attr_t *attr, char **name, size_t *len,
		  unsigned char **data, sig_val_t **sig,
		  struct acookie **cookie);


/* used publicly only by adiskd */
diamond_public
int obj_ref_attr(obj_attr_t *attr, const char * name, size_t *len,
		 unsigned char **data);

#endif /* ! _OBJ_ATTR_H_ */
