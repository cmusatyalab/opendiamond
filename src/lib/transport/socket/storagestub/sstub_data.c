/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2007 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2006-2008 Carnegie Mellon University
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
	int i;

	for (i = 0; i < output_set->len; i++) {
		if (q == g_array_index(output_set, GQuark, i))
			return 1;
	}
	return 0;
}

int sstub_get_attributes(obj_attr_t *obj_attr, GArray *output_set,
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
	err = obj_first_attr(obj_attr, NULL, NULL, NULL, NULL, &cookie);
	for (n = 0; err == 0; n++)
		err = obj_next_attr(obj_attr, NULL, NULL, NULL, NULL, &cookie);

	attrs = malloc(n * sizeof(attribute_x));
	if (n != 0 && attrs == NULL)
		return -1;

	err = obj_first_attr(obj_attr, &name, &len, (unsigned char **)&data,
			     NULL, &cookie);
	for (n = 0; err == 0; n++)
	{
		int send_data = !output_set || is_array_member(output_set, name);

		attrs[n].name = name;
		attrs[n].data.data_len = send_data ? len : 0;
		attrs[n].data.data_val = send_data ? data : NULL;

		err = obj_next_attr(obj_attr, &name, &len,
				    (unsigned char **)&data, NULL, &cookie);
	}
	*result_val = attrs;
	*result_len = n;
	return 0;
}

void
sstub_send_objects(cstate_t *cstate)
{
	obj_data_t	*obj;
	int		err;
	object_x	object;
	unsigned int	n;
	mrpc_status_t	rc;
	unsigned int	tx_hdr_bytes;
	unsigned int	tx_data_bytes;

next_obj:
	pthread_mutex_lock(&cstate->cmutex);
	if (cstate->cc_credits <= 0) {
		pthread_mutex_unlock(&cstate->cmutex);
		return;
	}

	obj = ring_deq(cstate->complete_obj_ring);
	/* If we don't get a complete object, look for a partial. */
	if (!obj)
		obj = ring_deq(cstate->partial_obj_ring);

	if (!obj) {
		pthread_mutex_unlock(&cstate->cmutex);
		return;
	}

	/* decrement credit count */
	cstate->cc_credits--;
	pthread_mutex_unlock(&cstate->cmutex);

	object.search_id = cstate->search_id;
	object.object.object_len = 0;
	object.object.object_val = NULL;

	/* The 'main' object data we return contains the object's contents
	 * if no thumbnail set was specified */
	if (!cstate->thumbnail_set) {
		obj_ref_attr(&obj->attr_info, OBJ_DATA,
			     &object.object.object_len,
			     (unsigned char **)&object.object.object_val);
		obj_omit_attr(&obj->attr_info, OBJ_DATA);
	}

	tx_hdr_bytes = sizeof(object_x);
	tx_data_bytes = object.object.object_len;

	err = sstub_get_attributes(&obj->attr_info, cstate->thumbnail_set,
				   &object.attrs.attrs_val,
				   &object.attrs.attrs_len);
	if (err) goto drop;

	for (n = 0; n < object.attrs.attrs_len; n++)
	{
		tx_hdr_bytes += sizeof(attribute_x);
		tx_data_bytes += object.attrs.attrs_val[n].data.data_len;
	}

	rc = blast_channel_send_object(cstate->blast_conn, &object);
	assert(rc == MINIRPC_OK);

	cstate->stats_objs_tx++;
	cstate->stats_objs_hdr_bytes_tx += tx_hdr_bytes;
	cstate->stats_objs_data_bytes_tx += tx_data_bytes;
	cstate->stats_objs_total_bytes_tx += tx_hdr_bytes + tx_data_bytes;

drop:
	/* dont' use free_object_x because we do not want to free the
	 * referenced object attributes */
	free(object.attrs.attrs_val); /* this is the only malloc'ed bit */

	/*
	 * If we make it here, then we have successfully sent
	 * the object so we need to make sure our state is set
	 * to no data pending, and we will call the callback the frees
	 * the object.
	 */
	(*cstate->lstate->cb.release_obj_cb) (cstate->app_cookie, obj);
	goto next_obj;
}

static void
update_credit(void *conn_data, struct mrpc_message *msg, credit_x *in)
{
	cstate_t *cstate = (cstate_t *)conn_data;

	pthread_mutex_lock(&cstate->cmutex);
	cstate->cc_credits = in->credits;
	pthread_mutex_unlock(&cstate->cmutex);

	mrpc_release_event();
	sstub_send_objects(cstate);
}

static const struct blast_channel_server_operations ops = {
	.update_credit = update_credit
};
const struct blast_channel_server_operations *sstub_blast_ops = &ops;

