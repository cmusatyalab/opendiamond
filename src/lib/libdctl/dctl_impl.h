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

#ifndef	_DCTL_IMPL_H_
#define	_DCTL_IMPL_H_	1

#include <sys/queue.h>


typedef struct dctl_leaf
{
	LIST_ENTRY(dctl_leaf)	leaf_link;
	char *			        leaf_name;
	dctl_data_type_t	    leaf_type;
	dctl_write_fn		    leaf_write_cb;
	dctl_read_fn		    leaf_read_cb;
	void *			        leaf_cookie;
}
dctl_leaf_t;

typedef enum {
    DCTL_NODE_LOCAL,
    DCTL_NODE_FWD,
} dctl_node_type_t;



typedef struct dctl_node
{
	dctl_node_type_t                    node_type;
	LIST_ENTRY(dctl_node)	            node_link;
	LIST_HEAD(node_children, dctl_node) node_children;
	LIST_HEAD(node_leafs, dctl_leaf)    node_leafs;
	char *			                    node_name;
	void *                              node_cookie;
	dctl_fwd_rleaf_fn                   node_rleaf_cb;
	dctl_fwd_wleaf_fn                   node_wleaf_cb;
	dctl_fwd_lnodes_fn                  node_lnodes_cb;
	dctl_fwd_lleafs_fn                  node_lleafs_cb;
}
dctl_node_t;

int dctl_unregister_node(char *path, char *node_name);

int dctl_unregister_leaf(char *path, char *leaf_name);

int dctl_register_fwd_node(char *parent_node, char *node_name,
				  dctl_fwd_cbs_t *cbs);
int dctl_unregister_fwd_node(char *parent_node, char *node_name);

int dctl_read_uint64(void *cookie, int *len, char *data);
int dctl_write_uint64(void *cookie, int len, char *data);
int dctl_read_char(void *cookie, int *len, char *data);
int dctl_write_char(void *cookie, int len, char *data);
int dctl_read_string(void *cookie, int *len, char *data);



#endif	/* !defined(_DCTL_IMPL_H_) */


