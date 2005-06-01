/*
 * 	Diamond (Release 1.0)
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

#ifndef	_LIB_ODISK_H_
#define	_LIB_ODISK_H_ 	1

#include <dirent.h>
#include "obj_attr.h"

#ifdef	__cplusplus
extern "C"
{
#endif

	struct odisk_state;

	/* maybe we need to remove this later */
#define MAX_DIR_PATH    128
#define MAX_GID_FILTER  64

	typedef struct odisk_state {
		char            odisk_path[MAX_DIR_PATH];
		groupid_t       gid_list[MAX_GID_FILTER];
		FILE *          index_files[MAX_GID_FILTER];
		int             num_gids;
		int             max_files;
		int             cur_file;
		pthread_t       thread_id;
		DIR *           odisk_dir;
		void *          dctl_cookie;
		void *          log_cookie;
		uint32_t        obj_load;
		uint32_t        next_blocked;
		uint32_t        readahead_full;
	}
	odisk_state_t;

	typedef struct gid_list {
		int         num_gids;
		groupid_t   gids[0];
	}
	gid_list_t;

#define GIDLIST_SIZE(num)   (sizeof(gid_list_t) + (sizeof(groupid_t) * (num)))

#define GIDLIST_NAME        "gid_list"

	/*
	 * This is the state associated with the object
	 */
	typedef struct obj_data {
		off_t			data_len;
		off_t			cur_offset;
		uint64_t       	local_id;
		int		    	cur_blocksize;
		int		    	ref_count;
		pthread_mutex_t	mutex;
		float			remain_compute;
		char *			data;
		char *			base;
		obj_attr_t		attr_info;
	}
	obj_data_t;

	typedef struct {
		obj_data_t *        obj;
		int                 ver_num;
	}
	obj_info_t;

	typedef struct {
		uint64_t obj_id;
		char **filters;
		char **fsig;
		char **iattrsig;
		int oattr_fnum;
		u_int64_t       stack_ns;
	}
	pr_obj_t;

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
	int odisk_release_obj(obj_data_t *obj);
	void odisk_ref_obj(obj_data_t *obj);
	int odisk_set_gid(struct odisk_state *odisk, groupid_t gid);
	int odisk_clear_gids(struct odisk_state *odisk);
	int odisk_reset(struct odisk_state *odisk);

	int odisk_clear_indexes(odisk_state_t * odisk);
	int odisk_build_indexes(odisk_state_t * odisk);
	int odisk_num_waiting(struct odisk_state *odisk);
	int odisk_load_obj(odisk_state_t *odisk, obj_data_t **o_handle, char *name);

	int odisk_delete_obj(struct odisk_state *odisk, obj_data_t *obj);

	int odisk_get_attr_sig(obj_data_t *obj, const char *name, char *data, int len);

	float odisk_get_erate(struct odisk_state *odisk);

	obj_data_t     * odisk_null_obj();


	/* JIAYING */
	int odisk_read_next_oid(uint64_t *oid, odisk_state_t *odisk);
	int odisk_pr_add(pr_obj_t *pr_obj);
	int odisk_flush(odisk_state_t *odisk);

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_ODISK_H */

