#ifndef	_LIB_OD_H
#define	_LIB_OD_H 	    1


#ifdef	__cplusplus
extern "C" {
#endif

struct obj_id {
    uint64_t    dev_id;
    uint64_t    local_id;
};

typedef struct obj_id obj_id_t;

typedef uint64_t    groupid_t;

#ifndef RPCGEN

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


int od_set_attr(obj_id_t *oid, char *name, int len, char *buf);

/*
 * Delete an attribute on an object.
 */
int od_del_attr(obj_id_t *oid, char *name);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* !_LIB_OD_H */

