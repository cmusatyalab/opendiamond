/*
 * 	Diamond (Release 1.0)
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

/*
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */


#ifndef _CONSTS_H_
#define _CONSTS_H_

/* some constants pertaining to the maximum filter values */ 
#define	MAX_FILTER_NAME		128
#define	MAX_FILTER_FUNC_NAME	64

/* maximum number of filters we support */
#define	MAX_FILTERS		64

/*
 * XXX this is getting pretty big. there should be a better way? 
 */
#define	MAX_NUM_DEPS		20


/* default path names for the different configurations files */
#define DIAMOND_CONFIG_ENV_NAME                 "DIAMOND_CONFIG"
#define DIAMOND_CONFIG_DIR_NAME                 ".diamond"
#define DIAMOND_CONFIG_FILE_NAME                "diamond_config"

/* file extension for the text attributes and binary attributes */
#define	BIN_ATTR_EXT				".attr"
#define	TEXT_ATTR_EXT				".text_attr"

/* maximum attribute name we allow */
#define	MAX_ATTR_NAME		128

/* default location of diamond cache if not set in .diamondrc */
#define	DEFAULT_DIAMOND_CACHE	"/tmp/diamond_cache"

/* directory under diamond cache where binaries are stored */
#define	BINARY_DIAMOND_CACHE	"binary"

/* directory under diamond cache where filter specs are stored */
#define	SPEC_DIAMOND_CACHE	"specs"

#define	OBJ_FORMAT		"%s/%s.obj"
#define	SO_FORMAT		"%s/%s.so"
#define	SPEC_FORMAT		"%s/%s.spec"


/* maximum name of an attribute file in the cache*/
#define	MAX_ATTR_CACHE_NAME		512

#endif                          /* _CONSTS_H_ */
