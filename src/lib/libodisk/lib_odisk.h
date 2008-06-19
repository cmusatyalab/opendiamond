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

#ifndef	_LIB_ODISK_H_
#define	_LIB_ODISK_H_ 	1

#include <dirent.h>
#include <unistd.h>
#include <glib.h>
#include <stdbool.h>
#include "diamond_consts.h"

#ifdef	__cplusplus
extern "C"
{
#endif

#define MAX_GID_NAME    128

#define GID_IDX         "GIDIDX"

typedef struct pr_obj pr_obj_t;
typedef struct obj_data obj_data_t;
typedef struct odisk_state odisk_state_t;
typedef struct session_variables_state session_variables_state_t;

diamond_public
int odisk_init(struct odisk_state **odisk, char *path_name);

diamond_public
int odisk_get_obj_cnt(struct odisk_state *odisk);

diamond_public
int odisk_next_obj(obj_data_t **new_obj, struct odisk_state *odisk);

diamond_public
int odisk_release_obj(obj_data_t *obj);

diamond_public
int odisk_set_gid(struct odisk_state *odisk, groupid_t gid);

diamond_public
int odisk_clear_gids(struct odisk_state *odisk);

diamond_public
int odisk_reset(struct odisk_state *odisk);

diamond_public
int odisk_num_waiting(struct odisk_state *odisk);

diamond_public
float odisk_get_erate(struct odisk_state *odisk);

diamond_public
obj_data_t     * odisk_null_obj(void);

diamond_public
int odisk_flush(odisk_state_t *odisk);

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_ODISK_H */

