#ifndef _OBJ_ATTR_H_
#define _OBJ_ATTR_H_


/*
 * This is the state associated with the object
 */
typedef struct {
	off_t			attr_len;
	char *			attr_data;
} obj_attr_t;


/*
 * XXX we need to store these in network byte order and fixed
 * size for sharing across the network.
 */


typedef struct attr_record {
	int		rec_len;
	int		name_len;
	int		data_len;
	int		flags;
	char 		data[4];
} attr_record_t;

#define	ATTR_FLAG_FREE		0x01
#define	ATTR_FLAG_RECOMPUTE	0x02

/* constant for the extend increment size */
#define	ATTR_INCREMENT	4096
#define	ATTR_MIN_FRAG	64


/*
 * The extension on a file that shows it is an attribute.
 */
#define	ATTR_EXT	".attr"

/*
 * The maximum lenght of the name string for an attribute.
 */
#define	MAX_ATTR_NAME	128


/*
 * These are the object attribute managment calls.
 */
int obj_write_attr(obj_attr_t *attr, const char *name,
			  off_t len, const char *data);
int obj_read_attr(obj_attr_t *attr, const char *name,
			 off_t *len, char *data);
int obj_del_attr(obj_attr_t *attr, const char *name);
int obj_read_attr_file(char *attr_fname, obj_attr_t *attr);
int obj_write_attr_file(char *attr_fname, obj_attr_t *attr);

int obj_get_attr_first(obj_attr_t *attr, char **buf, size_t *len, 
	void **cookie);

int obj_get_attr_next(obj_attr_t *attr, char **buf, size_t *len, 
	void **cookie);

#endif
