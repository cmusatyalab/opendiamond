/*
 * 	Diamond
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

#ifndef	_DCTL_COMMON_H_
#define	_DCTL_COMMON_H_	1


/*
 * This header file file contains some of the common path
 * names that are used to access resources dctl tree.
 */

#define	ROOT_PATH			""
#define	HOST_PATH			"host"

#define	HOST_BACKGROUND			"bg"
#define	HOST_BACKGROUND_PATH		"host.bg"

#define	HOST_NETWORK_NODE		"network"
#define	HOST_NETWORK_PATH		"host.network"



#define	DEVICE_PATH			"devices"
#define	HOST_DEVICE_PATH		"devices"


#define	SEARCH_NAME			"cur_search"
#define	DEV_SEARCH_PATH			"cur_search"


#define	DEV_NETWORK_NODE		"network"
#define	DEV_NETWORK_PATH		"network"

#define	DEV_FEXEC_NODE			"fexec"
#define	DEV_FEXEC_PATH			"fexec"

#define	DEV_OBJ_NODE			"obj_disk"
#define	DEV_OBJ_PATH			"obj_disk"

#define	DEV_CACHE_NODE			"cache"
#define	DEV_CACHE_PATH			"cache"

#endif	/* !defined(_DCTL_COMMON_H_) */


