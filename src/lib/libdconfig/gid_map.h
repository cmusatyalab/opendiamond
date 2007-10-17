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

#ifndef _GID_MAP_H_
#define _GID_MAP_H_	1

typedef struct dev_info
{
	char           *dev_name;
	uint32_t        dev_id;
}
dev_info_t;

#define	MAX_DEV_PER_GROUP	32
typedef struct gid_map
{
	struct gid_map *next;
	groupid_t       gid;
	int             num_dev;
	dev_info_t      devs[MAX_DEV_PER_GROUP];
}
gid_map_t;

#endif	/* !_GID_MAP_H_ */
