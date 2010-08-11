/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#ifndef _SYS_ATTR_H_
#define _SYS_ATTR_H_

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

#define OBJ_ID		"_ObjectID"
#define OBJ_DATA	""

#define DISPLAY_NAME    "Display-Name"
#define DEVICE_NAME     "Device-Name"

#define OBJ_PATH        "_path.cstring"
#define FLTRTIME        "_FIL_TIME.time"
#define FLTRTIME_FN     "_FIL_TIME_%s.time"
#define PERMEABILITY_FN "_FIL_STAT_%s_permeability.float"


#endif /* _SYS_ATTR_H_ */
