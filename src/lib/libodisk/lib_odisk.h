/*
 *
 *
 *                          Diamond 1.0
 * 
 *            Copyright (c) 2002-2004, Intel Corporation
 *                         All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of Intel nor the names of its contributors may
 *      be used to endorse or promote products derived from this software 
 *      without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_LIB_ODISK_H_
#define	_LIB_ODISK_H_ 	1

#include <dirent.h>
#include "obj_attr.h"

#ifdef	__cplusplus
extern "C" {
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
} odisk_state_t;

typedef struct gid_list {
    int         num_gids;
    groupid_t   gids[0];
} gid_list_t;

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
} obj_data_t;

typedef struct {
    obj_data_t *        obj;
    int                 ver_num;
} obj_info_t;

typedef struct {
   uint64_t obj_id;
   char **filters;
   char **fsig;
   char **iattrsig;
   int oattr_fnum;
   u_int64_t       stack_ns;	
} pr_obj_t;
 
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

                                                                                
obj_data_t     * odisk_null_obj();


/* JIAYING */
int odisk_read_next_oid(uint64_t *oid, odisk_state_t *odisk);
int odisk_pr_add(pr_obj_t *pr_obj);
int odisk_flush(odisk_state_t *odisk);

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_ODISK_H */

