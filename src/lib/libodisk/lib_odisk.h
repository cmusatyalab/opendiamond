#ifndef	_LIB_ODISK_H_
#define	_LIB_ODISK_H_ 	1

#include "obj_attr.h"


struct odisk_state;


/*
 * This is the state associated with the object
 */
typedef struct {
	off_t			data_len;
	off_t			cur_offset;
	int			cur_blocksize;
	char *			data;
	obj_attr_t		attr_info;
} obj_data_t;


/*
 * These are the function prototypes for the device emulation
 * function in dev_emul.c
 */
extern int odisk_term(struct odisk_state *odisk);
extern int odisk_init(struct odisk_state **odisk, char *path_name);
extern int odisk_get_obj_cnt(struct odisk_state *odisk);
extern int odisk_next_obj(obj_data_t **new_obj, struct odisk_state *odisk);

#endif	/* !_LIB_ODISK_H */

