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

#ifndef	_LIB_OD_H
#define	_LIB_OD_H 	    1


#ifdef	__cplusplus
extern "C" {
#endif

/* XXX hack */
#ifdef	RPCGEN
typedef	uint64_t	groupid_t;


struct obj_id {
    uint64_t    dev_id;
    uint64_t    local_id;
};

typedef struct obj_id obj_id_t;

#endif

#ifndef RPCGEN

#include "diamond_types.h" 

/*
 * call before calling the other library calls.
 *
 */
int od_init();

/*
 * Create a new object. the resulting object ID is returned
 * in "new_obj" if the call is succesful.   A return value of
 * 0 indicates success.
 */
int od_create(obj_id_t *new_obj, groupid_t* gid);


/*
 * Delete an existin object.  oid, contains the object ID of 
 * the ID to delete.
 */
int od_delete(obj_id_t *oid);

/*
 * Read a range of bytes from the offset and len specified
 * in the call.  The data is stored in "buf".  The return values
 * is the number of bytes read.  -1 indicates the operation failed.
 * If less than the specified number of bytes is returned, the end
 * of the file has been reached.
 */
int od_read(obj_id_t *oid, off_t offset, off_t len, char *buf);

/*
 * Write to the specified object at the offset and length specified.
 * The resulting data is returned in the buffer.
 */
int od_write(obj_id_t *oid, off_t offset, off_t len, char *buf);

/*
 * Add a new groupid to the specified object. 
 */
int od_add_gid(obj_id_t *oid, groupid_t * gid);

/*
 * Remove a new groupid to the specified object. 
 */
int od_rem_gid(obj_id_t *oid, groupid_t * gid);

/*
 * Get the attribute from the specified object.  The name is the name
 * of the attribute.  Upon calling, len should be the length of the buffer.
 * ON return len will be the mount of data returned.  A return value
 * of 0 indicates sucess.  A return of ENOMEM indicates the buffer
 * was no large enought to complete the request.  In this case, len is
 * set to be the size needed to complete the call. 
 */
int od_get_attr(obj_id_t *oid, char *name, int *len, char *buf);

/*
 * Set a an attribute on the specified object.
 */


int od_set_attr(obj_id_t *oid, const char *name, int len, const char *buf);

/*
 * Delete an attribute on an object.
 */
int od_del_attr(obj_id_t *oid, char *name);

/*
 * Read a GID index file, hack bootstraping for now.
 */
int od_read_gididx(groupid_t gid, uint32_t devid, off_t offset, 
	off_t len, char *buf);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_OD_H */

