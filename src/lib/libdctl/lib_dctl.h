#ifndef	_LIB_DCTL_H_
#define	_LIB_DCTL_H_	1


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

/*
 * These are the function prototypes that are associated with the
 * read and write operations for the given data node.
 */
typedef int (*dctl_read_fn)(void *cookie, int *data_len, char *data);
typedef int (*dctl_write_fn)(void *cookie, int data_len, char *data);

/*
 * These are the function prototypes for the callback functions used
 * when establishing a "mount" point.
 */
typedef int (*dctl_fwd_rleaf_fn)(char *leaf_name, dctl_data_type_t *dtype,
                int *len, char *data, void *cookie);
typedef int (*dctl_fwd_wleaf_fn)(char *leaf_name, int len, char *data,
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



extern int dctl_init();
extern int dctl_register_node(char *path, char *node_name);
extern int dctl_unregister_node(char *path, char *node_name);
extern int dctl_register_leaf(char *path, char *leaf_name, 
		dctl_data_type_t dctl_data_t, dctl_read_fn read_cb, 
		dctl_write_fn write_cb, void *cookie);
extern int dctl_unregister_leaf(char *path, char *leaf_name);

extern int dctl_read_leaf(char *leaf_name, dctl_data_type_t *type, 
                int *len, char *data);
extern int dctl_write_leaf(char *leaf_name, int len, char *data);

extern int dctl_list_nodes(char *parent_node, int *num_ents, dctl_entry_t *
		entry_space);
extern int dctl_list_leafs(char *parent_node, int *num_ents, dctl_entry_t *
		entry_space);

extern int dctl_register_fwd_node(char *parent_node, char *node_name,
                dctl_fwd_cbs_t *cbs);
extern int dctl_unregister_fwd_node(char *parent_node, char *node_name);



/*
 * The following are a set of helper functions for reading, writing
 * commoon data types.  The callers can use these are the read and
 * write functions passed to dctl_register_leaf().  The cookie must
 * be the pointer to the data of the appropriate type.
 */
int dctl_read_uint32(void *cookie, int *len, char *data);
int dctl_write_uint32(void *cookie, int len, char *data);
int dctl_read_uint64(void *cookie, int *len, char *data);
int dctl_write_uint64(void *cookie, int len, char *data);
int dctl_read_char(void *cookie, int *len, char *data);
int dctl_write_char(void *cookie, int len, char *data);
int dctl_read_string(void *cookie, int *len, char *data);


#endif	/* !defined(_LIB_DCTL_H_) */


