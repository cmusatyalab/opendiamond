#ifndef	_LIB_ODISK_H_
#define	_LIB_ODISK_H_ 	1

#include "obj_attr.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct odisk_state;

typedef struct gid_list {
    int         num_gids;
    groupid_t   gids[0];
} gid_list_t;

#define GIDLIST_SIZE(num)   (sizeof(gid_list_t) + (sizeof(groupid_t) * (num)))

#define GIDLIST_NAME        "gid_list"

/*
 * This is the state associated with the object
 */
typedef struct {
	off_t			data_len;
	off_t			cur_offset;
    	uint64_t        	local_id;
	int		    	cur_blocksize;
	char *			data;
	obj_attr_t		attr_info;
} obj_data_t;

typedef struct {
    obj_data_t *        obj;
    int                 ver_num;
} obj_info_t;


/*
 * These are the function prototypes for the device emulation
 * function in dev_emul.c
 */
int odisk_term(struct odisk_state *odisk);
int odisk_init(struct odisk_state **odisk, char *path_name,
	void *dctl_cookie, void * log_cookie);
int odisk_get_obj_cnt(struct odisk_state *odisk);
int odisk_next_obj(obj_data_t **new_obj, struct odisk_state *odisk);

int odisk_get_obj(struct odisk_state *odisk, obj_data_t **new_obj, 
                obj_id_t *oid);

int odisk_new_obj(struct odisk_state *odisk, obj_id_t *oid, groupid_t *gid);

int odisk_save_obj(struct odisk_state *odisk, obj_data_t *obj);

int odisk_write_obj(struct odisk_state *odisk, obj_data_t *obj, int len,
                int offset, char *buf);

int odisk_read_obj(struct odisk_state *odisk, obj_data_t *obj, int *len,
                int offset, char *buf);

int odisk_add_gid(struct odisk_state *odisk, obj_data_t *obj, groupid_t *gid);
int odisk_rem_gid(struct odisk_state *odisk, obj_data_t *obj, groupid_t *gid);
int odisk_release_obj(struct odisk_state *odisk, obj_data_t *obj);
int odisk_set_gid(struct odisk_state *odisk, groupid_t gid);
int odisk_clear_gids(struct odisk_state *odisk);
int odisk_reset(struct odisk_state *odisk);

int odisk_num_waiting(struct odisk_state *odisk);

int odisk_delete_obj(struct odisk_state *odisk, obj_data_t *obj);



#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_ODISK_H */

