/*
 *      Diamond (Release 1.0)
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

#include "lib_dctl.h"
#include "queue.h"
#include "dctl_impl.h"

static char const cvsid[] =
    "$Header$";

#define	MAX_TOKEN	128

/*
 * some state for handling our multiple instantiations 
 */
static pthread_key_t root_node_key;
static pthread_once_t root_node_once = PTHREAD_ONCE_INIT;

static pthread_mutex_t dctl_mutex;


static dctl_node_t *
get_root_node()
{
	dctl_node_t    *rn;

	rn = (dctl_node_t *) pthread_getspecific(root_node_key);
	assert(rn != NULL);
	return (rn);
}


void
dctl_thread_register(void *cookie)
{
	pthread_setspecific(root_node_key, (char *) cookie);
}


static dctl_node_t *
lookup_child_node(dctl_node_t * pnode, char *node_name)
{
	dctl_node_t    *cur_node;

	LIST_FOREACH(cur_node, &pnode->node_children, node_link) {
		if (strcmp(cur_node->node_name, node_name) == 0) {
			return (cur_node);
		}
	}
	return (NULL);
}


static void
insert_node(dctl_node_t * parent_node, dctl_node_t * new_node)
{
	LIST_INSERT_HEAD(&parent_node->node_children, new_node, node_link);
}

static void
remove_node(dctl_node_t * parent_node, dctl_node_t * old_node)
{
	LIST_REMOVE(old_node, node_link);
}

static void
insert_leaf(dctl_node_t * parent_node, dctl_leaf_t * new_leaf)
{
	LIST_INSERT_HEAD(&parent_node->node_leafs, new_leaf, leaf_link);
}

static void
remove_leaf(dctl_node_t * parent_node, dctl_leaf_t * old_leaf)
{
	LIST_REMOVE(old_leaf, leaf_link);
}


static dctl_leaf_t *
lookup_child_leaf(dctl_node_t * parent_node, char *leaf_name)
{
	dctl_leaf_t    *cur_leaf;

	LIST_FOREACH(cur_leaf, &parent_node->node_leafs, leaf_link) {
		if (strcmp(cur_leaf->leaf_name, leaf_name) == 0) {
			return (cur_leaf);
		}
	}
	return (NULL);
}


static dctl_node_t *
lookup_node(char *node_name)
{
	char           *head;
	char           *delim;
	int             len;
	char           *next_head;
	dctl_node_t    *cur_node;
	char            token_data[MAX_TOKEN];

	/*
	 * try to see if this is a lits of items.
	 */

	cur_node = get_root_node();
	head = node_name;
	while ((head != NULL) && (*head != '\0')) {
		delim = index(head, '.');
		if (delim == NULL) {
			len = strlen(head);
			next_head = NULL;
		} else {
			len = delim - head;
			next_head = delim + 1;
		}
		assert(len < MAX_TOKEN);

		strncpy(token_data, head, len);
		token_data[len] = 0;

		cur_node = lookup_child_node(cur_node, token_data);
		if (cur_node == NULL) {
			return (NULL);
		}

		head = next_head;
	}

	return (cur_node);
}


#ifdef	XXX
static dctl_leaf_t *
lookup_leaf(char *leaf_name)
{
	char           *head;
	char           *delim;
	int             len;
	char           *next_head;
	dctl_node_t    *cur_node;
	dctl_leaf_t    *leaf;
	char            token_data[MAX_TOKEN];

	/*
	 * try to see if this is a lits of items.
	 */

	cur_node = get_root_node();
	head = leaf_name;
	while ((head != NULL) && (*head != '\0')) {
		delim = index(head, '.');
		if (delim == NULL) {
			leaf_name = head;
			break;
		} else {
			len = delim - head;
			next_head = delim + 1;
		}
		assert(len < MAX_TOKEN);

		strncpy(token_data, head, len);
		token_data[len] = 0;

		cur_node = lookup_child_node(cur_node, token_data);
		if (cur_node == NULL) {
			return (NULL);
		}

		head = next_head;
	}

	leaf = lookup_child_leaf(cur_node, leaf_name);

	return (leaf);
}

#endif


static void
root_key_alloc()
{
	pthread_key_create(&root_node_key, NULL);
}


int
dctl_init(void **cookie)
{
	int             err;
	dctl_node_t    *root_node;

	pthread_once(&root_node_once, root_key_alloc);

	if (pthread_getspecific(root_node_key) != NULL) {
		return (ENOENT);	/* XXX different error ?? */
	}

	root_node = (dctl_node_t *) malloc(sizeof(*root_node));
	if (root_node == NULL) {
		return (ENOMEM);	/* XXX different error ?? */
	}

	LIST_INIT(&root_node->node_leafs);
	LIST_INIT(&root_node->node_children);

	root_node->node_name = "root";

	err = pthread_mutex_init(&dctl_mutex, NULL);
	assert(err == 0);

	pthread_setspecific(root_node_key, (char *) root_node);
	*cookie = root_node;
	return (0);
}

int
dctl_register_fwd_node(char *path, char *node_name, dctl_fwd_cbs_t * cbs)
{
	dctl_node_t    *parent_node;
	dctl_node_t    *new_node;
	dctl_leaf_t    *leaf_probe;
	int             len;

	len = strlen(path);

	/*
	 * find the parent node for this entry 
	 */
	parent_node = lookup_node(path);
	if (parent_node == NULL) {
		return (ENOENT);
	}

	/*
	 * If the node name exists and is of the correct type we just
	 * replace the state because we may do this if we have to re-start
	 * the search at a node.
	 */
	new_node = lookup_child_node(parent_node, node_name);
	if (new_node != NULL) {
		if (new_node->node_type != DCTL_NODE_FWD) {
			return (EEXIST);
		}
		new_node->node_rleaf_cb = cbs->dfwd_rleaf_cb;
		new_node->node_wleaf_cb = cbs->dfwd_wleaf_cb;
		new_node->node_lnodes_cb = cbs->dfwd_lnodes_cb;
		new_node->node_lleafs_cb = cbs->dfwd_lleafs_cb;
		new_node->node_cookie = cbs->dfwd_cookie;
		return (0);
	}

	/*
	 * make sure there is not a leaf with this name 
	 */
	leaf_probe = lookup_child_leaf(parent_node, node_name);
	if (leaf_probe != NULL) {
		return (EEXIST);
	}


	/*
	 * allocate the new node and setup it up 
	 */
	new_node = (dctl_node_t *) malloc(sizeof(*new_node));
	if (new_node == NULL) {
		return (ENOMEM);	/* XXX different error ?? */
	}

	new_node->node_type = DCTL_NODE_FWD;
	new_node->node_name = strdup(node_name);
	if (new_node->node_name == NULL) {
		free(new_node);
		return (ENOMEM);	/* XXX different error ?? */
	}

	/*
	 * store the callback functions 
	 */
	new_node->node_rleaf_cb = cbs->dfwd_rleaf_cb;
	new_node->node_wleaf_cb = cbs->dfwd_wleaf_cb;
	new_node->node_lnodes_cb = cbs->dfwd_lnodes_cb;
	new_node->node_lleafs_cb = cbs->dfwd_lleafs_cb;
	new_node->node_cookie = cbs->dfwd_cookie;

	/*
	 * initialize the lists for the children of this node 
	 */
	LIST_INIT(&new_node->node_leafs);
	LIST_INIT(&new_node->node_children);


	/*
	 * insert this node onto the parents list 
	 */
	insert_node(parent_node, new_node);

	return (0);
}

int
dctl_unregister_fwd_node(char *path, char *node_name)
{
	dctl_node_t    *parent_node;
	dctl_node_t    *new_node;
	int             len;

	len = strlen(path);

	parent_node = lookup_node(path);
	if (parent_node == NULL) {
		return (ENOENT);
	}

	/*
	 * find the node we are deleteing 
	 */
	new_node = lookup_child_node(parent_node, node_name);
	if (new_node == NULL) {
		return (ENOENT);
	}

	/*
	 * make sure nobody depends on this node 
	 */
	if ((!LIST_EMPTY(&new_node->node_children)) ||
	    (!LIST_EMPTY(&new_node->node_children))) {
		return (EBUSY);
	}

	/*
	 * remove the node from the list 
	 */
	remove_node(parent_node, new_node);

	/*
	 * free the associated resources 
	 */
	free(new_node->node_name);
	free(new_node);

	return (0);
}

int
dctl_register_node(char *path, char *node_name)
{
	dctl_node_t    *parent_node;
	dctl_node_t    *new_node;
	dctl_leaf_t    *leaf_probe;
	int             len;

	len = strlen(path);

	/*
	 * find the parent node for this entry 
	 */
	parent_node = lookup_node(path);
	if (parent_node == NULL) {
		return (ENOENT);
	}

	/*
	 * If the entry exists and is a node we just return okay.  If
	 * it isn't a node this is an error.
	 */
	new_node = lookup_child_node(parent_node, node_name);
	if (new_node != NULL) {
		if (new_node->node_type == DCTL_NODE_LOCAL) {
			return(0);
		} else {
			return(EEXIST);
		}
	}

	/*
	 * make sure there is not a leaf with this name 
	 */
	leaf_probe = lookup_child_leaf(parent_node, node_name);
	if (leaf_probe != NULL) {
		return (EEXIST);
	}


	/*
	 * allocate the new node and setup it up 
	 */
	new_node = (dctl_node_t *) malloc(sizeof(*new_node));
	if (new_node == NULL) {
		return (ENOMEM);	/* XXX different error ?? */
	}

	new_node->node_type = DCTL_NODE_LOCAL;
	new_node->node_name = strdup(node_name);
	if (new_node->node_name == NULL) {
		free(new_node);
		return (ENOMEM);	/* XXX different error ?? */
	}

	/*
	 * initialize the lists for the children of this node 
	 */
	LIST_INIT(&new_node->node_leafs);
	LIST_INIT(&new_node->node_children);

	/*
	 * insert this node onto the parents list 
	 */
	insert_node(parent_node, new_node);

	return (0);
}


int
dctl_unregister_node(char *path, char *node_name)
{
	dctl_node_t    *parent_node;
	dctl_node_t    *new_node;
	int             len;

	len = strlen(path);

	parent_node = lookup_node(path);
	if (parent_node == NULL) {
		return (ENOENT);
	}

	/*
	 * find the node we are deleteing 
	 */
	new_node = lookup_child_node(parent_node, node_name);
	if (new_node == NULL) {
		return (ENOENT);
	}

	/*
	 * make sure nobody depends on this node 
	 */
	if ((!LIST_EMPTY(&new_node->node_children)) ||
	    (!LIST_EMPTY(&new_node->node_children))) {
		return (EBUSY);
	}

	/*
	 * remove the node from the list 
	 */
	remove_node(parent_node, new_node);

	/*
	 * free the associated resources 
	 */
	free(new_node->node_name);
	free(new_node);

	return (0);
}

/*
 * This registers a leaf node with the appropriate read/write functions.
 * We have changed behavior to replace entries instead of failing if
 * they already exist.
 */

int
dctl_register_leaf(char *path, char *leaf_name, dctl_data_type_t data_type,
		   dctl_read_fn read_cb, dctl_write_fn write_cb, void *cookie)
{
	dctl_node_t    *parent_node;
	dctl_leaf_t    *new_leaf;

	parent_node = lookup_node(path);
	if (parent_node == NULL) {
		return (ENOENT);
	}

	/*
	 * make sure this name doesn't already exist 
	 */
	new_leaf = lookup_child_leaf(parent_node, leaf_name);
	if (new_leaf == NULL) {
		new_leaf = (dctl_leaf_t *) malloc(sizeof(*new_leaf));
		if (new_leaf == NULL) {
			return (ENOMEM);	/* XXX different error ?? */
		}

		new_leaf->leaf_name = strdup(leaf_name);
		if (new_leaf->leaf_name == NULL) {
			free(new_leaf);
			return (ENOMEM);	/* XXX different error ?? */
		}
		insert_leaf(parent_node, new_leaf);
	}
	new_leaf->leaf_type = data_type;
	new_leaf->leaf_read_cb = read_cb;
	new_leaf->leaf_write_cb = write_cb;
	new_leaf->leaf_cookie = cookie;


	return (0);
}




int
dctl_unregister_leaf(char *path, char *leaf_name)
{
	dctl_node_t    *parent_node;
	dctl_leaf_t    *new_leaf;

	parent_node = lookup_node(path);

	if (parent_node == NULL) {
		return (ENOENT);
	}

	/*
	 * find the node we are deleteing 
	 */
	new_leaf = lookup_child_leaf(parent_node, leaf_name);
	if (new_leaf == NULL) {
		return (ENOENT);
	}

	/*
	 * remove the node from the list 
	 */
	remove_leaf(parent_node, new_leaf);

	/*
	 * free the associated resources 
	 */
	free(new_leaf->leaf_name);
	free(new_leaf);

	return (0);
}

int
dctl_list_nodes(char *path, int *num_ents, dctl_entry_t * entry_space)
{
	dctl_node_t    *pnode;
	dctl_node_t    *cur_node;
	int             i;
	int             err;
	char           *head;
	char           *delim;
	int             len;
	char           *next_head;
	char            token_data[MAX_TOKEN];


	/*
	 * try to see if this is a lits of items.
	 */

	pnode = get_root_node();
	head = path;
	while (*head != '\0') {
		delim = index(head, '.');
		if (delim == NULL) {
			len = strlen(head);
			next_head = head + len;
		} else {
			len = delim - head;
			next_head = delim + 1;
		}
		assert(len < MAX_TOKEN);

		strncpy(token_data, head, len);
		token_data[len] = 0;

		pnode = lookup_child_node(pnode, token_data);
		if (pnode == NULL) {
			return (ENOENT);
		}

		/*
		 * See if this is a mount point.  If so,
		 * then we call the appropriate callback function
		 * and return the results.
		 */
		if (pnode->node_type == DCTL_NODE_FWD) {
			err = (*pnode->node_lnodes_cb) (next_head, num_ents,
							entry_space,
							pnode->node_cookie);
			return (err);
		}

		head = next_head;
	}


	/*
	 * see how many entries we have and if the caller provided
	 * enough space.
	 */
	i = 0;
	LIST_FOREACH(cur_node, &pnode->node_children, node_link) {
		i++;
	}
	if (i > *num_ents) {
		*num_ents = i;
		return (ENOMEM);
	}

	i = 0;
	LIST_FOREACH(cur_node, &pnode->node_children, node_link) {
		strncpy(entry_space[i].entry_name, cur_node->node_name,
			MAX_DCTL_COMP_NAME);
		entry_space[i].entry_type = DCTL_DT_NODE;
		i++;
	}


	*num_ents = i;
	return (0);
}

int
dctl_list_leafs(char *path, int *num_ents, dctl_entry_t * entry_space)
{
	dctl_node_t    *pnode;
	dctl_leaf_t    *cur_leaf;
	int             i;
	int             err;
	char           *head;
	char           *delim;
	int             len;
	char           *next_head;
	char            token_data[MAX_TOKEN];


	/*
	 * try to see if this is a lits of items.
	 */

	pnode = get_root_node();
	head = path;
	while (*head != '\0') {
		delim = index(head, '.');
		if (delim == NULL) {
			len = strlen(head);
			next_head = head + len;
		} else {
			len = delim - head;
			next_head = delim + 1;
		}
		assert(len < MAX_TOKEN);

		strncpy(token_data, head, len);
		token_data[len] = 0;

		pnode = lookup_child_node(pnode, token_data);
		if (pnode == NULL) {
			return (ENOENT);
		}

		/*
		 * See if this is a mount point.  If so,
		 * then we call the appropriate callback function
		 * and return the results.
		 */
		if (pnode->node_type == DCTL_NODE_FWD) {
			err = (*pnode->node_lleafs_cb) (next_head, num_ents,
							entry_space,
							pnode->node_cookie);
			return (err);
		}

		head = next_head;
	}


	/*
	 * see how many entries we have and if the caller provided
	 * enough space.
	 */
	i = 0;
	LIST_FOREACH(cur_leaf, &pnode->node_leafs, leaf_link) {
		i++;
	}
	if (i > *num_ents) {
		*num_ents = i;
		return (ENOMEM);
	}

	i = 0;
	LIST_FOREACH(cur_leaf, &pnode->node_leafs, leaf_link) {
		strncpy(entry_space[i].entry_name, cur_leaf->leaf_name,
			MAX_DCTL_COMP_NAME);
		entry_space[i].entry_type = cur_leaf->leaf_type;
		i++;
	}


	*num_ents = i;
	return (0);
}



int
dctl_read_leaf(char *leaf_name, dctl_data_type_t * dtype, int *len,
	       char *data)
{
	dctl_leaf_t    *leaf;
	dctl_node_t    *pnode;
	int             err;
	char           *head;
	char           *delim;
	int             clen;
	char           *next_head;
	char            token_data[MAX_TOKEN];

	/*
	 * try to see if this is a lits of items.
	 */

	pnode = get_root_node();
	head = leaf_name;
	while (*head != '\0') {
		delim = index(head, '.');
		if (delim == NULL) {
			leaf_name = head;
			break;
		} else {
			clen = delim - head;
			next_head = delim + 1;
		}
		assert(clen < MAX_TOKEN);

		strncpy(token_data, head, clen);
		token_data[clen] = 0;

		pnode = lookup_child_node(pnode, token_data);
		if (pnode == NULL) {
			return (ENOENT);
		}

		/*
		 * See if this is a mount point.  If so,
		 * then we call the appropriate callback function
		 * and return the results.
		 */
		if (pnode->node_type == DCTL_NODE_FWD) {
			err = (*pnode->node_rleaf_cb) (next_head, dtype, len,
						       data,
						       pnode->node_cookie);
			return (err);
		}

		head = next_head;
	}

	leaf = lookup_child_leaf(pnode, leaf_name);
	if (leaf == NULL) {
		return (ENOENT);
	}
	*dtype = leaf->leaf_type;
	err = (*leaf->leaf_read_cb) (leaf->leaf_cookie, len, data);
	return (err);
}



int
dctl_write_leaf(char *leaf_name, int len, char *data)
{
	dctl_leaf_t    *leaf;
	dctl_node_t    *pnode;
	int             err;
	char           *head;
	char           *delim;
	int             clen;
	char           *next_head;
	char            token_data[MAX_TOKEN];

	/*
	 * try to see if this is a lits of items.
	 */

	pnode = get_root_node();
	head = leaf_name;
	while (*head != '\0') {
		delim = index(head, '.');
		if (delim == NULL) {
			leaf_name = head;
			break;
		} else {
			clen = delim - head;
			next_head = delim + 1;
		}
		assert(clen < MAX_TOKEN);

		strncpy(token_data, head, clen);
		token_data[clen] = 0;

		pnode = lookup_child_node(pnode, token_data);
		if (pnode == NULL) {
			return (ENOENT);
		}

		/*
		 * See if this is a mount point.  If so,
		 * then we call the appropriate callback function
		 * and return the results.
		 */
		if (pnode->node_type == DCTL_NODE_FWD) {
			err = (*pnode->node_wleaf_cb) (next_head, len,
						       data,
						       pnode->node_cookie);
			return (err);
		}

		head = next_head;
	}

	/*
	 * find the leaf node we are writing 
	 */
	leaf = lookup_child_leaf(pnode, leaf_name);
	if (leaf == NULL) {
		return (ENOENT);
	}

	/*
	 * if there is not write callback, this is a read only variable 
	 */
	if (leaf->leaf_write_cb == NULL) {
		return (EPERM);
	}
	err = (*leaf->leaf_write_cb) (leaf->leaf_cookie, len, data);
	return (err);
}
