/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2009 Carnegie Mellon University
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
#define MAX_HOST_NAME	255

struct odisk_state {
	unsigned int	search_id;
	GPtrArray	*scope;
	char		odisk_name[MAX_HOST_NAME];
	pthread_t       thread_id;
	uint32_t        obj_load;
	uint32_t        next_blocked;

	GAsyncQueue	*queue;
	gint		fetchers;
	uint64_t        count;
};

struct session_variables_state {
	pthread_mutex_t mutex;
	GHashTable     *store;
	bool            between_get_and_set;
};


struct pr_obj {
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
	sig_val_t      		id_sig;
	int		    	ref_count;
	pthread_mutex_t	mutex;
	obj_attr_t		attr_info;
	session_variables_state_t *session_variables_state;
	intptr_t		dev_cookie;
};


/*
 * These are the function prototypes for the device emulation
 * function in dev_emul.c
 */
int odisk_continue(void);




int odisk_get_attr_sig(obj_data_t *obj, const char *name, sig_val_t*sig);


char * odisk_next_obj_name(odisk_state_t *odisk);
int odisk_pr_add(pr_obj_t *pr_obj);


/* dataretriever.c */
void dataretriever_init(const char *base_uri);
void dataretriever_start_search(odisk_state_t *odisk);
void dataretriever_stop_search(odisk_state_t *odisk);
char *dataretriever_next_object_uri(odisk_state_t *odisk);
obj_data_t *dataretriever_fetch_object(const char *uri_string);

#endif	/* !_ODISK_PRIV_H_ */

