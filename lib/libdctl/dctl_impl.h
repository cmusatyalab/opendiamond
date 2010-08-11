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

#ifndef	_DCTL_IMPL_H_
#define	_DCTL_IMPL_H_	1

#include <sys/queue.h>
#include <fcntl.h>

#include "diamond_features.h"



typedef enum {
    DCTL_DT_NODE = 1,
    DCTL_DT_UINT32,
    DCTL_DT_UINT64,
    DCTL_DT_STRING,
    DCTL_DT_CHAR
} dctl_data_type_t;

/* the maximum name of any dctl component (node or leaf) */
#define	MAX_DCTL_COMP_NAME	64


typedef	struct {
    dctl_data_type_t	entry_type;
    char 			    entry_name[MAX_DCTL_COMP_NAME];
} dctl_entry_t;


typedef struct {
  dctl_data_type_t  dt;
  size_t            len;
  char             *dbuf;
} dctl_rleaf_t;

typedef struct {
  int               err;
  int               num_ents;
  dctl_entry_t     *ent_data;
} dctl_lleaf_t;

typedef struct {
  int               err;
  int               num_ents;
  dctl_entry_t     *ent_data;
} dctl_lnode_t;

/*
 * These are the function prototypes that are associated with the
 * read and write operations for the given data node.
 */
typedef int (*dctl_read_fn)(void *cookie, size_t *data_len, char *data);
typedef int (*dctl_write_fn)(void *cookie, size_t data_len, char *data);

/*
 * These are the function prototypes for the callback functions used
 * when establishing a "mount" point.
 */
typedef int (*dctl_fwd_rleaf_fn)(char *leaf_name, dctl_data_type_t *dtype,
				 size_t *len, char *data, void *cookie);
typedef int (*dctl_fwd_wleaf_fn)(char *leaf_name, size_t len, char *data,
				 void *cookie);
typedef int (*dctl_fwd_lnodes_fn)(char *parent_node, int *num_ents,
	                                  dctl_entry_t *entry_space, void *cookie);
typedef int (*dctl_fwd_lleafs_fn)(char *parent_node, int *num_ents,
				  dctl_entry_t *entry_space, void *cookie);


typedef	struct {
    dctl_fwd_rleaf_fn   dfwd_rleaf_cb;
    dctl_fwd_wleaf_fn   dfwd_wleaf_cb;
    dctl_fwd_lnodes_fn  dfwd_lnodes_cb;
    dctl_fwd_lleafs_fn  dfwd_lleafs_cb;
    void *              dfwd_cookie;
} dctl_fwd_cbs_t;


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

diamond_public
int dctl_register_node(char *path, char *node_name);

diamond_public
int dctl_register_leaf(char *path, char *leaf_name, dctl_data_type_t data_type,
		       dctl_read_fn read_cb, dctl_write_fn write_cb, void *cookie);

diamond_public
int dctl_read_leaf(char *leaf_name, dctl_data_type_t *type,
		   size_t *len, char *data);
diamond_public
int dctl_write_leaf(char *leaf_name, size_t len, char *data);

diamond_public
int dctl_list_nodes(char *parent_node, int *num_ents, dctl_entry_t *
		    entry_space);

diamond_public
int dctl_list_leafs(char *parent_node, int *num_ents, dctl_entry_t *
		    entry_space);

/*
 * The following are helper functions for reading, writing common data types.
 * The callers can use these are the read and write functions passed to
 * dctl_register_leaf().  The cookie must be the pointer to the data of the
 * appropriate type.
 */
diamond_public
void dctl_register_u32(char *path, char *leaf, int mode, uint32_t *item);

diamond_public
void dctl_register_u64(char *path, char *leaf, int mode, uint64_t *item);

#endif	/* !defined(_DCTL_IMPL_H_) */

