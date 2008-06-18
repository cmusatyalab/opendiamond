/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef	_ODISK_PRIV_H_
#define	_ODISK_PRIV_H_ 	1


struct odisk_state;


/*
 * XXX we need to clean up this interface so this is not externally 
 * visible.
 */

/*
 * Some macros for using the O_DIRECT call for aligned buffer
 * management.
 */

/* alignment restriction */
#define	OBJ_ALIGN	4096
#define	ALIGN_MASK	(~(OBJ_ALIGN -1))

#define	ALIGN_SIZE(sz)	((sz) + (2 * OBJ_ALIGN))
#define	ALIGN_VAL(base)	(void*)(((uint32_t)(base)+ OBJ_ALIGN - 1) & ALIGN_MASK)
#define	ALIGN_ROUND(sz)	(((sz) + OBJ_ALIGN - 1) & ALIGN_MASK)

/* some maintence functions */
int odisk_write_oids(odisk_state_t * odisk, uint32_t devid);

void obj_load_text_attr(odisk_state_t *odisk, char *file_name, 
	obj_data_t *new_obj);


/*
 * These are the function prototypes for the device emulation
 * function in dev_emul.c
 */
int odisk_term(struct odisk_state *odisk);
int odisk_continue(void);





int odisk_get_obj(struct odisk_state *odisk, obj_data_t **new_obj,
		  obj_id_t *oid);

int odisk_new_obj(struct odisk_state *odisk, obj_id_t *oid, groupid_t *gid);

int odisk_save_obj(struct odisk_state *odisk, obj_data_t *obj);

int odisk_write_obj(struct odisk_state *odisk, obj_data_t *obj, int len,
		    int offset, char *buf);

int odisk_read_obj(struct odisk_state *odisk, obj_data_t *obj, int *len,
		   int offset, char *buf);

int odisk_add_gid(struct odisk_state *odisk, obj_data_t *obj, groupid_t *gid);
int odisk_rem_gid(struct odisk_state *odisk, obj_data_t *obj, groupid_t *gid);

void odisk_ref_obj(obj_data_t *obj);


int odisk_clear_indexes(odisk_state_t * odisk);
int odisk_build_indexes(odisk_state_t * odisk);

int odisk_load_obj(odisk_state_t *odisk, obj_data_t **o_handle, char *name);

int odisk_delete_obj(struct odisk_state *odisk, obj_data_t *obj);

int odisk_get_attr_sig(obj_data_t *obj, const char *name, sig_val_t*sig);


char * odisk_next_obj_name(odisk_state_t *odisk);
int odisk_pr_add(pr_obj_t *pr_obj);


/*
 * These are the object attribute managment calls.
 */
int obj_write_attr(obj_attr_t *attr, const char *name,
                   size_t len, const unsigned char *data);
int obj_read_attr(obj_attr_t *attr, const char *name,
                  size_t *len, unsigned char *data);



int obj_omit_attr(obj_attr_t *attr, const char *name);
int obj_del_attr(obj_attr_t *attr, const char *name);
int obj_read_attr_file(struct odisk_state *odisk, char *attr_fname, 
		obj_attr_t *attr);
int obj_write_attr_file(char *attr_fname, obj_attr_t *attr);

int obj_get_attr_first(obj_attr_t *attr, unsigned char **buf, size_t *len,
                       void **cookie, int skip_big);

int obj_get_attr_next(obj_attr_t *attr, unsigned char **buf, size_t *len,
                      void **cookie, int skip_big);

int obj_first_attr(obj_attr_t * attr, char **name, size_t * len, 
		unsigned char **data, void **cookie);
int obj_next_attr(obj_attr_t * attr, char **name, size_t * len, 
		unsigned char **data, void **cookie);


attr_record_t * odisk_get_arec(struct obj_data *obj, const char *name);




#endif	/* !_ODISK_PRIV_H_ */

