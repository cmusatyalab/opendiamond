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

#ifndef	_ODISK_PRIV_H_
#define	_ODISK_PRIV_H_ 	1


struct odisk_state;


#define	MAX_GID_NAME	128

#define	GID_IDX		"GIDIDX"

/*
 * XXX we need to clean up this interface so this is not externally 
 * visible.
 */

/* for now, moved this to lib_odisk. but do we want to keep in private?
#define	MAX_DIR_PATH	128
#define	MAX_GID_FILTER	64
typedef struct odisk_state {
	char		odisk_path[MAX_DIR_PATH];
	groupid_t	gid_list[MAX_GID_FILTER];
	FILE *		index_files[MAX_GID_FILTER];
	int		num_gids;
	int		max_files;
	int		cur_file;
	pthread_t	thread_id;
	DIR *		odisk_dir;
	void *		dctl_cookie;
	void *		log_cookie;
	uint32_t	obj_load;
	uint32_t	next_blocked;
	uint32_t	readahead_full;
} odisk_state_t;
*/

typedef	struct gid_idx_ent {
	char		gid_name[MAX_GID_NAME];
} gid_idx_ent_t;


/*
 * Some macros for using the O_DIRECT call for aligned buffer
 * management.
 */

/* alignment restriction */
#define	OBJ_ALIGN	4096
#define	ALIGN_MASK	(~(OBJ_ALIGN -1))

#define	ALIGN_SIZE(sz)	((sz) + (2 * OBJ_ALIGN))
#define	ALIGN_VAL(base)	(void*)(((uint32_t)(base)+ OBJ_ALIGN - 1) & ALIGN_MASK)
#define	ALIGN_ROUND(sz)	(((sz) + OBJ_ALIGN - 1) & ALIGN_MASK)


#endif	/* !_ODISK_PRIV_H_ */

