
#ifndef _ATTR_H_
#define _ATTR_H_

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
#define PERMEABILITY_FN "_FIL_STAT_%s_permeability.float"


#endif /* _ATTR_H_ */
