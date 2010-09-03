/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * These file handles a lot of the device specific code.  For the current
 * version we have state for each of the devices.
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "obj_attr.h"
#include "lib_odisk.h"
#include "socket_trans.h"
#include "lib_sstub.h"
#include "sstub_impl.h"
#include "odisk_priv.h"
#include "sys_attr.h"

#include "blast_channel_server.h"

static int is_array_member(GArray *output_set, const char *item)
{
	GQuark q = g_quark_from_string(item);
	unsigned int i;

	for (i = 0; i < output_set->len; i++) {
		if (q == g_array_index(output_set, GQuark, i))
			return 1;
	}
	return 0;
}

int sstub_get_attributes(obj_data_t *obj, GArray *output_set,
			 attribute_x **result_val, unsigned int *result_len)
{
	struct acookie *cookie;
	attribute_x *attrs;
	unsigned int n;
	char *name;
	void *data;
	size_t len;
	int err;

	/* how many attributes are we sending? */
	err = obj_first_attr(&obj->attr_info, NULL, NULL, NULL, NULL, &cookie);
	for (n = 0; err == 0; n++)
		err = obj_next_attr(&obj->attr_info, NULL, NULL, NULL, NULL,
				    &cookie);

	attrs = malloc(n * sizeof(attribute_x));
	if (n != 0 && attrs == NULL)
		return -1;

	err = obj_first_attr(&obj->attr_info, &name, &len,
			     (unsigned char **)&data, NULL, &cookie);
	for (n = 0; err == 0; n++)
	{
		int senddata = !output_set || is_array_member(output_set, name);

		int size = senddata ? len : 0;
		void *buf = senddata ? malloc(size) : NULL;
		if (senddata) memcpy(buf, data, len);

		attrs[n].name = strdup(name);
		attrs[n].data.data_len = size;
		attrs[n].data.data_val = buf;

		err = obj_next_attr(&obj->attr_info, &name, &len,
				    (unsigned char **)&data, NULL, &cookie);
	}
	*result_val = attrs;
	*result_len = n;
	return 0;
}

static mrpc_status_t
get_object(void *conn_data, struct mrpc_message *msg, object_x *object)
{
	cstate_t *cstate = (cstate_t *)conn_data;
	obj_data_t *obj;
	int err;

	obj = g_async_queue_pop(cstate->complete_obj_ring);

	object->search_id = cstate->search_id;

	/* The 'main' object data we return contains the object's contents
	 * if no thumbnail set was specified */
	if (!cstate->thumbnail_set) {
		unsigned char *data;
		size_t len;

		if (obj_ref_attr(&obj->attr_info, OBJ_DATA, &len, &data) == 0) {
		    obj_omit_attr(&obj->attr_info, OBJ_DATA);

		    object->object.object_len = len;
		    object->object.object_val = malloc(len);
		    memcpy(object->object.object_val, data, len);
		}
	}

	err = sstub_get_attributes(obj, cstate->thumbnail_set,
				   &object->attrs.attrs_val,
				   &object->attrs.attrs_len);
	if (err) return DIAMOND_FAILURE;

	/*
	 * If we make it here, then we have successfully sent
	 * the object so we need to make sure our state is set
	 * to no data pending, and we will call the callback the frees
	 * the object.
	 */
	(*cstate->lstate->cb.release_obj_cb) (cstate->app_cookie, obj);

	return MINIRPC_OK;
}

static const struct blast_channel_server_operations ops = {
	.get_object = get_object
};
const struct blast_channel_server_operations *sstub_blast_ops = &ops;

