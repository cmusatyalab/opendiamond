#ifndef _OBJ_ATTR_H_
#define _OBJ_ATTR_H_


/*
 * This is the state associated with the object
 */
typedef struct {
	off_t			attr_len;
	char *			attr_data;
} obj_attr_t;




typedef struct attr_record {
	int		rec_len;
	int		name_len;
	int		data_len;
	int		flags;
	char 		data[4];
} attr_record_t;

#define	ATTR_FLAG_FREE		0x01

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
 * Names for some of the system defined attributes.
 * XXX update these from the spec.
 */

#define	SIZE		"SYS_SIZE"
#define	UID		"SYS_UID"
#define	GID		"SYS_GID"
#define	BLK_SIZE	"SYS_BLKSIZE"
#define	ATIME		"SYS_ATIME"
#define	MTIME		"SYS_MTIME"
#define	CTIME		"SYS_CTIME"

#define OBJ_PATH        "_path.cstring"
#define FLTRTIME        "_FIL_TIME.time"
#define FLTRTIME_FN     "_FIL_TIME_%s.time"


/*
 * These are the object attribute managment calls.
 */
extern int obj_write_attr(obj_attr_t *attr, const char *name,
			  off_t len, const char *data);
extern int obj_read_attr(obj_attr_t *attr, const char *name,
			 off_t *len, char *data);
extern int obj_del_attr(obj_attr_t *attr, const char *name);
extern int obj_read_attr_file(char *attr_fname, obj_attr_t *attr);
extern int obj_write_attr_file(char *attr_fname, obj_attr_t *attr);

#endif
