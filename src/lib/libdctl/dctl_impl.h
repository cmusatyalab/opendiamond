#ifndef	_DCTL_IMPL_H_
#define	_DCTL_IMPL_H_	1



typedef struct dctl_leaf {
	LIST_ENTRY(dctl_leaf)	leaf_link;
	char *			leaf_name;
	dctl_data_type_t	leaf_type;
	dctl_write_fn		leaf_write_cb;
	dctl_read_fn		leaf_read_cb;
	void *			leaf_cookie;
} dctl_leaf_t;

typedef struct dctl_node {
	LIST_ENTRY(dctl_node)	node_link;
	LIST_HEAD(node_children, dctl_node) node_children;
	LIST_HEAD(node_leafs, dctl_leaf) node_leafs;
	/* struct dctl_leaf *	node_leafs; */
	char *			node_name;
} dctl_node_t;

	
#endif	/* !defined(DCTL_IMPL_H_) */


