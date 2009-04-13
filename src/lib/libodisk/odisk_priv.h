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

#ifndef	_ODISK_PRIV_H_
#define	_ODISK_PRIV_H_ 	1

#include <glib.h>
#include "obj_attr.h"

typedef struct gid_idx_ent {
    char                gid_name[MAX_GID_NAME];
} gid_idx_ent_t;


/* maybe we need to remove this later */
#define MAX_DIR_PATH    512
#define MAX_GID_FILTER  64
#define MAX_HOST_NAME	255

struct odisk_state {
	char            odisk_indexdir[MAX_DIR_PATH];
	groupid_t       gid_list[MAX_GID_FILTER];
	char		odisk_name[MAX_HOST_NAME];
	int             num_gids;
	pthread_t       thread_id;
	uint32_t        obj_load;
	uint32_t        next_blocked;

	GAsyncQueue	*queue;
	gint		fetchers;
	uint64_t        count;
};

typedef struct gid_list {
	int         num_gids;
	groupid_t   gids[0];
} gid_list_t;

#define GIDLIST_SIZE(num)   (sizeof(gid_list_t) + (sizeof(groupid_t) * (num)))

#define GIDLIST_NAME        "gid_list"


struct session_variables_state {
	pthread_mutex_t mutex;
	GHashTable     *store;
	bool            between_get_and_set;
};


struct pr_obj {
	uint64_t 	obj_id;
	char *		obj_name;
	int 		oattr_fnum;
	char *		filters[MAX_FILTERS];
	int64_t		filter_hits[MAX_FILTERS];
	u_int64_t       stack_ns;
};


	/*
	 * This is the state associated with the object
	 */
struct obj_data {
	off_t			cur_offset;
	sig_val_t      		id_sig;
	int		    	cur_blocksize;
	int		    	ref_count;
	pthread_mutex_t	mutex;
	float			remain_compute;
	obj_attr_t		attr_info;
	session_variables_state_t *session_variables_state;
	intptr_t		dev_cookie;
};


/*
 * These are the function prototypes for the device emulation
 * function in dev_emul.c
 */
int odisk_term(struct odisk_state *odisk);
int odisk_continue(void);




void odisk_ref_obj(obj_data_t *obj);

int odisk_get_attr_sig(obj_data_t *obj, const char *name, sig_val_t*sig);


char * odisk_next_obj_name(odisk_state_t *odisk);
int odisk_pr_add(pr_obj_t *pr_obj);


attr_record_t * odisk_get_arec(struct obj_data *obj, const char *name);

/* dataretriever.c */
void dataretriever_init(const char *base_uri);
void dataretriever_start_search(odisk_state_t *odisk);
void dataretriever_stop_search(odisk_state_t *odisk);
char *dataretriever_next_object_uri(odisk_state_t *odisk);
obj_data_t *dataretriever_fetch_object(const char *uri_string);

#endif	/* !_ODISK_PRIV_H_ */

