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

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <assert.h>
/* #include "lib_od.h"  */
#include "queue.h"
#include "od.h"
#include "od_priv.h"
#include "od_priv.h"



/*
 * call before calling the other library calls.
 *
 */
int
od_init()
{
	ods_init();
	return(0);
}

/*
 * Create a new object. the resulting object ID is returned
 * in "new_obj" if the call is succesful.   A return value of
 * 0 indicates success.
 */
int
od_create(obj_id_t *new_obj, groupid_t* gid)
{
	od_srv_t *          osrv;
	create_obj_arg_t    cobj;
	create_obj_result_t *cresult;


	osrv = ods_allocate_by_gid(gid);
	if (osrv == NULL) {
		return(ENOENT);
	}

	cobj.gid = gid;

	cresult = rpc_create_obj_1(&cobj, osrv->ods_client);
	if (cresult == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}


	if (cresult->status != 0) {
		fprintf(stderr, "failed to create on %d \n", cresult->status);
		return(cresult->status);
	}

	*new_obj = *(cresult->obj_id);
	new_obj->dev_id = osrv->ods_id;
	return(0);
}


/*
 * Delete an existin object.  oid, contains the object ID of 
 * the ID to delete.
 */
int
od_delete(obj_id_t *oid)
{
	/* XXX fix */
	return(0);
}

/*
 * Read a range of bytes from the offset and len specified
 * in the call.  The data is stored in "buf".  The return values
 * is the number of bytes read.  -1 indicates the operation failed.
 * If less than the specified number of bytes is returned, the end
 * of the file has been reached.
 */
int
od_read(obj_id_t *oid, off_t offset, off_t len, char *buf)
{
	od_srv_t *          osrv;
	read_data_arg_t     rdata;
	read_results_t *    results;

	osrv = ods_lookup_by_oid(oid);
	if (osrv == NULL) {
		return(ENOENT);
	}

	rdata.oid = oid;
	rdata.offset = offset;
	rdata.len = len;


	results = rpc_read_data_1(&rdata, osrv->ods_client);
	if (results == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}


	if (results->status != 0) {
		fprintf(stderr, "failed to read on %d \n", results->status);
		return(-1);
	}
	memcpy(buf, results->data.data_val, results->data.data_len);

	return(results->data.data_len);
}

/*
 * Write to the specified object at the offset and length specified.
 * The resulting data is returned in the buffer.
 */
int
od_write(obj_id_t *oid, off_t offset, off_t len, char *buf)
{
	od_srv_t *          osrv;
	write_data_arg_t    wdata;
	int *               result;

	osrv = ods_lookup_by_oid(oid);
	if (osrv == NULL) {
		return(ENOENT);
	}

	wdata.oid = oid;
	wdata.offset = offset;
	wdata.data.data_len = len;
	wdata.data.data_val = buf;

	result = rpc_write_data_1(&wdata, osrv->ods_client);
	if (result == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}


	if (*result != 0) {
		fprintf(stderr, "failed to write on %d \n", *result);
		return(*result);
	}

	return(0);
}


/*
 * Add a new groupid to the specified object. 
 */
int
od_add_gid(obj_id_t *oid, groupid_t * gid)
{
	od_srv_t *          osrv;
	update_gid_args_t   ugid;
	int *               result;

	osrv = ods_lookup_by_oid(oid);
	if (osrv == NULL) {
		return(ENOENT);
	}

	ugid.oid = oid;
	ugid.gid = gid;


	result = rpc_add_gid_1(&ugid, osrv->ods_client);
	if (result == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}


	if (*result != 0) {
		fprintf(stderr, "failed add gid on %d \n", *result);
		return(*result);
	}

	return(0);
}

/*
 * Remove a new groupid to the specified object. 
 */
int
od_rem_gid(obj_id_t *oid, groupid_t * gid)
{
	od_srv_t *          osrv;
	update_gid_args_t   ugid;
	int *               result;


	osrv = ods_lookup_by_oid(oid);
	if (osrv == NULL) {
		return(ENOENT);
	}


	ugid.oid = oid;
	ugid.gid = gid;


	result = rpc_rem_gid_1(&ugid, osrv->ods_client);
	if (result == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}

	if (*result != 0) {
		fprintf(stderr, "failed rem gid on %d \n", *result);
	}

	return(*result);
}


/*
 * Get the attribute from the specified object.  The name is the name
 * of the attribute.  Upon calling, len should be the length of the buffer.
 * ON return len will be the mount of data returned.  A return value
 * of 0 indicates sucess.  A return of ENOMEM indicates the buffer
 * was no large enought to complete the request.  In this case, len is
 * set to be the size needed to complete the call. 
 */
int
od_get_attr(obj_id_t *oid, char *name, int *len, char *buf)
{
	od_srv_t *          osrv;
	rattr_args_t        rattr;
	read_results_t *    results;


	osrv = ods_lookup_by_oid(oid);
	if (osrv == NULL) {
		return(ENOENT);
	}

	rattr.oid = oid;
	rattr.name = name;

	results = rpc_read_attr_1(&rattr, osrv->ods_client);
	if (results == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}


	if (results->status != 0) {
		fprintf(stderr, "failed rem gid on %d \n", results->status);
		return(results->status);
	}

	if (*len > results->data.data_len) {
		memcpy(buf, results->data.data_val, results->data.data_len);
		*len = results->data.data_len;
		return(0);
	} else {
		*len = results->data.data_len;
		return(ENOMEM);
	}
}

/*
 * Set a an attribute on the specified object.
 */


int
od_set_attr(obj_id_t *oid, const char *name, int len, const char *buf)
{
	od_srv_t *          osrv;
	wattr_args_t        wattr;
	int *               result;


	osrv = ods_lookup_by_oid(oid);
	if (osrv == NULL) {
		return(ENOENT);
	}


	wattr.oid = oid;
	wattr.name = name;
	wattr.data.data_len = len;
	wattr.data.data_val = buf;

	result = rpc_write_attr_1(&wattr, osrv->ods_client);
	if (result == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}


	if (*result != 0) {
		fprintf(stderr, "failed rem gid on %d \n", *result);
	}

	return(*result);
}


/*
 * Delete an attribute on an object.
 */
int
od_del_attr(obj_id_t *oid, char *name)
{
	return(0);
}

/*
 * Read a range of bytes from the GID index file
 * The data is stored in "buf".  The return values
 * is the number of bytes read.  -1 indicates the operation failed.
 * If less than the specified number of bytes is returned, the end
 * of the file has been reached.
 */
int
od_read_gididx(groupid_t gid, uint32_t devid, off_t offset,
               off_t len, char *buf)
{
	od_srv_t *          osrv;
	rgid_idx_arg_t      rgid;
	rgid_results_t *    results;
	uint64_t		bigid;

	bigid = (uint64_t) devid;
	osrv = ods_lookup_by_devid(devid);
	if (osrv == NULL) {
		return(ENOENT);
	}

	rgid.gid = gid;
	rgid.offset = offset;
	rgid.len = len;


	results = rpc_read_gididx_1(&rgid, osrv->ods_client);
	if (results == NULL) {
		clnt_perror(osrv->ods_client, osrv->ods_name);
		exit(1);
	}

	if (results->status != 0) {
		fprintf(stderr, "failed to read on %d \n", results->status);
		return(-1);
	}
	memcpy(buf, results->data.data_val, results->data.data_len);

	return(results->data.data_len);
}



