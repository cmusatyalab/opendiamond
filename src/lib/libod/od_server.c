/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <assert.h>
#include <errno.h>
#include "od.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"



static struct odisk_state *odata = NULL;



/*
 * SHould be called the first time any of the operations is run.
 */
void
init_disk()
{
	int             err;
	void           *log_cookie;
	void           *dctl_cookie;

	log_init("od_server", NULL, &log_cookie);
	dctl_init(&dctl_cookie);

	err = odisk_init(&odata, NULL, dctl_cookie, log_cookie);
	if (err) {
		odata = NULL;
		fprintf(stderr, "unable to initialize odisk \n");
		exit(1);
	}
}




/*
 * Create a new object on the disk.
 */
create_obj_result_t *
rpc_create_obj_1_svc(create_obj_arg_t * arg, struct svc_req *rq)
{

	static create_obj_result_t result;
	static obj_id_t new_oid;
	int             err;

	if (odata == NULL) {
		init_disk();
	}

	err = odisk_new_obj(odata, &new_oid, arg->gid);

	result.obj_id = &new_oid;
	result.status = err;
	return (&result);
}



int            *
rpc_write_data_1_svc(write_data_arg_t * arg, struct svc_req *rq)
{
	static int      result;
	obj_data_t     *obj;
	int             err;

	if (odata == NULL) {
		init_disk();
	}

	err = odisk_get_obj(odata, &obj, arg->oid);
	if (err != 0) {
		result = err;
	} else {
		err =
		    odisk_write_obj(odata, obj, arg->data.data_len,
				    arg->offset, arg->data.data_val);
		if (err == 0) {
			err = odisk_save_obj(odata, obj);
		}
		result = err;
		odisk_release_obj(obj);
	}

	return (&result);
}


read_results_t *
rpc_read_data_1_svc(read_data_arg_t * arg, struct svc_req * rq)
{

	static read_results_t result;
	int             len;
	int             err;
	static char    *buf = NULL;
	obj_data_t     *obj;

	if (odata == NULL) {
		init_disk();
	}

	if (buf != NULL) {
		free(buf);
		buf = NULL;
	}
	err = odisk_get_obj(odata, &obj, arg->oid);
	if (err != 0) {
		result.status = err;
	} else {
		buf = (char *) malloc(arg->len);
		len = arg->len;
		err = odisk_read_obj(odata, obj, &len, arg->offset, buf);
		if (err) {
			result.status = err;
		} else {
			result.status = 0;
			result.data.data_len = len;
			result.data.data_val = buf;
		}
		odisk_release_obj(obj);
	}
	return (&result);
}

int            *
rpc_add_gid_1_svc(update_gid_args_t * arg, struct svc_req *rq)
{
	static int      result;
	obj_data_t     *obj;
	int             err;

	if (odata == NULL) {
		init_disk();
	}

	err = odisk_get_obj(odata, &obj, arg->oid);
	if (err != 0) {
		result = err;
	} else {
		err = odisk_add_gid(odata, obj, arg->gid);
		result = err;
		err = odisk_save_obj(odata, obj);
		assert(err == 0);
		odisk_release_obj(obj);
	}

	return (&result);
}


int            *
rpc_rem_gid_1_svc(update_gid_args_t * arg, struct svc_req *rq)
{
	static int      result;
	obj_data_t     *obj;
	int             err;

	if (odata == NULL) {
		init_disk();
	}

	err = odisk_get_obj(odata, &obj, arg->oid);
	if (err != 0) {
		result = err;
	} else {
		err = odisk_rem_gid(odata, obj, arg->gid);
		result = err;
		err = odisk_save_obj(odata, obj);
		assert(err == 0);
		odisk_release_obj(obj);
	}

	return (&result);
}



int            *
rpc_write_attr_1_svc(wattr_args_t * arg, struct svc_req *rq)
{
	static int      result;
	obj_data_t     *obj;
	int             err;

	if (odata == NULL) {
		init_disk();
	}

	err = odisk_get_obj(odata, &obj, arg->oid);
	if (err != 0) {
		result = err;
		return (&result);
	}

	err = obj_write_attr(&obj->attr_info, arg->name, arg->data.data_len,
			     arg->data.data_val);
	if (err) {
		result = err;
		return (&result);
	}

	err = odisk_save_obj(odata, obj);
	assert(err == 0);
	odisk_release_obj(obj);

	result = 0;
	return (&result);
}

read_results_t *
rpc_read_attr_1_svc(rattr_args_t * arg, struct svc_req * rq)
{
	static read_results_t result;
	obj_data_t     *obj;
	int             err;
	off_t           len;
	static char    *dbuf = NULL;

	if (odata == NULL) {
		init_disk();
	}

	if (dbuf != NULL) {
		free(dbuf);
		dbuf = NULL;
	}

	err = odisk_get_obj(odata, &obj, arg->oid);
	if (err != 0) {
		result.status = err;
		result.data.data_len = 0;
		result.data.data_val = NULL;
		return (&result);
	}

	len = 0;
	err = obj_read_attr(&obj->attr_info, arg->name, &len, NULL);
	if (err == 0) {
		result.status = 0;
		result.data.data_len = 0;
		result.data.data_val = NULL;
		goto done;
	} else if (err != ENOMEM) {
		result.status = err;
		result.data.data_len = 0;
		result.data.data_val = NULL;
		goto done;
	}

	/*
	 * Allocate some space and read the real data.
	 */
	dbuf = (char *) malloc(len);
	assert(dbuf != NULL);

	err = obj_read_attr(&obj->attr_info, arg->name, &len, dbuf);
	if (err) {
		odisk_release_obj(obj);
		result.status = err;
		result.data.data_len = 0;
		result.data.data_val = NULL;
		free(dbuf);
		dbuf = NULL;
		goto done;
	}

	result.status = 0;
	result.data.data_len = len;
	result.data.data_val = dbuf;

      done:
	odisk_release_obj(obj);
	return (&result);
}


rgid_results_t *
rpc_read_gididx_1_svc(rgid_idx_arg_t * arg, struct svc_req * rq)
{
	char            idx_file[NAME_MAX];
	FILE           *new_file;
	static rgid_results_t result;
	int             len;
	static char    *buf = NULL;

	if (odata == NULL) {
		init_disk();
	}

	if (buf != NULL) {
		free(buf);
		buf = NULL;
	}

	len = snprintf(idx_file, NAME_MAX, "%s/%s%016llX",
		       odata->odisk_indexdir, "GIDIDX", arg->gid);
	assert(len < NAME_MAX);
	new_file = fopen(idx_file, "r");
	if (new_file == NULL) {
		perror("open");
		result.status = ENOENT;
	} else {

		buf = (char *) malloc(arg->len);
		len = arg->len;

		fseek(new_file, arg->offset, SEEK_SET);

		len = fread(buf, 1, len, new_file);
		result.status = 0;
		result.data.data_len = len;
		result.data.data_val = buf;
		fclose(new_file);
	}
	return (&result);
}
