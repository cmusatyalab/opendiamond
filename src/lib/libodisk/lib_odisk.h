#ifndef	_LIB_ODISK_H
#define	_LIB_ODISK_H_ 	1


typedef struct {
	void *		iter_cookie;
} odisk_state_t;


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
extern int odisk_term(odisk_state_t *odisk);
extern int odisk_init(odisk_state_t **odisk);
extern int odisk_next_obj(obj_data_t **new_obj, odisk_state_t *odisk);

#endif	/* !_LIB_ODISK_H */

