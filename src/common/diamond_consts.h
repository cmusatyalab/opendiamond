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


#ifndef _CONSTS_H_
#define _CONSTS_H_


#define	MAX_FILTER_NAME			128
#define	MAX_FILTER_FUNC_NAME	64

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

#endif                          /* _CONSTS_H_ */
