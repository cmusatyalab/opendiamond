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

#ifndef	_LIB_HSTUB_H_
#define	_LIB_HSTUB_H_


typedef	void (*hstub_log_data_fn)(void *hcookie, char *data, int len, int dev);
typedef	void (*hstub_search_done_fn)(void *hcookie, int ver_num);
typedef	void (*hstub_rleaf_done_fn)(void *hcookie, int err, 
                dctl_data_type_t dtype, int len, char *data, int32_t opid);
typedef	void (*hstub_wleaf_done_fn)(void *hcookie, int err, int32_t opid);
typedef	void (*hstub_lnodes_done_fn)(void *hcookie, int err, int num_ents,
                dctl_entry_t *data, int32_t opid);
typedef	void (*hstub_lleafs_done_fn)(void *hcookie, int err, int num_ents,
                dctl_entry_t *data, int32_t opid);


typedef struct {
	hstub_log_data_fn		    log_data_cb;
	hstub_search_done_fn	    search_done_cb;
	hstub_rleaf_done_fn		    rleaf_done_cb;
	hstub_wleaf_done_fn		    wleaf_done_cb;
	hstub_lnodes_done_fn		lnode_done_cb;
	hstub_lleafs_done_fn		lleaf_done_cb;
} hstub_cb_args_t;


/*
 * This structure keeps track of the state associated with each
 * of the storage devices.
 */

void *	device_init(int id, uint32_t devid, void *hcookie, 
			hstub_cb_args_t *cbs, void *dctl_cookie, 
			void *log_cookie);

int device_stop(void *dev, int id);
int device_terminate(void *dev, int id);
int device_start(void *dev, int id);
int device_set_searchlet(void *dev, int id, char *filter, char *spec);
int device_characteristics(void *handle, device_char_t *dev_chars);
int device_statistics(void *dev, dev_stats_t *dev_stats, 
		int *stat_len);
int device_set_log(void *handle, uint32_t level, uint32_t src);

int device_write_leaf(void *dev, char *path, int len, char *data,
                int32_t opid);
int device_read_leaf(void *dev, char *path, int32_t opid);
int device_list_nodes(void *dev, char *path, int32_t opid);
int device_list_leafs(void *dev, char *path, int32_t opid);
int device_new_gid(void *handle, int id, groupid_t gid);
int device_clear_gids(void *handle, int id);
int device_set_blob(void *handle, int id, char *name, int blob_len, void *blob);
int device_stop_obj(void *handle);
int device_enable_obj(void *handle);
int device_set_offload(void *handle, int id, uint64_t offload);
int device_set_limit(void *handle, int limit);
obj_info_t * device_next_obj(void *handle);


#endif	/* _LIB_HSTUB_H_ */



