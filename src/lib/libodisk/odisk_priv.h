#ifndef	_ODISK_PRIV_H_
#define	_ODISK_PRIV_H_ 	1


struct odisk_state;


#define	MAX_DIR_PATH	128
#define	MAX_GID_NAME	128

#define	GID_IDX		"GIDIDX"

/*
 * XXX we need to clean up this interface so this is not externally 
 * visible.
 */
typedef struct odisk_state {
	char		odisk_path[MAX_DIR_PATH];
	DIR *		odisk_dir;
} odisk_state_t;


typedef	struct gid_idx_ent {
	char		gid_name[MAX_GID_NAME];
} gid_idx_ent_t;


#endif	/* !_ODISK_PRIV_H_ */

