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
#include <sys/stat.h>
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
#include <assert.h>
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "socket_trans.h"
#include "obj_attr.h"
#include "lib_log.h"
#include "lib_odisk.h"
#include "lib_hstub.h"
#include "hstub_impl.h"
#include "odisk_priv.h"
#include "sys_attr.h"

#include "blast_channel_client.h"

/* This is called when adiskd sends a new object */
static void
recv_object(void *conn_data, struct mrpc_message *msg, object_x *object)
{
	sdevice_state_t *dev = (sdevice_state_t *)conn_data;
	conn_info_t	*cinfo = &dev->con_data;
	obj_data_t	*obj;
	attribute_x	*attr;
	size_t		obj_len, hdr_len, attr_len = 0;
	unsigned int	i;
	int		err;

	/* is this a result from a previous search? */
	if (object->search_id != dev->search_id) {
		log_message(LOGT_NET, LOGL_INFO,
			    "recv_object: dropping object from another search");
		return;
	}

	/* allocate an obj_data_t structure to hold the object. */
	obj = odisk_null_obj();
	assert(obj != NULL);

	obj_len = object->object.object_len;
	obj_write_attr(&obj->attr_info, OBJ_DATA, obj_len,
		       (unsigned char *)object->object.object_val);
	obj_omit_attr(&obj->attr_info, OBJ_DATA);

	hdr_len = sizeof(object_x);

	for (i = 0; i < object->attrs.attrs_len; i++) {
		attr = &object->attrs.attrs_val[i];
		err = obj_write_attr(&obj->attr_info, attr->name,
				     attr->data.data_len,
				     (unsigned char *)attr->data.data_val);
		if (err) {
			log_message(LOGT_NET, LOGL_CRIT,
				    "recv_object: obj_write_attr failed");
			return;
		}
		hdr_len += sizeof(attribute_x);
		attr_len += strlen(attr->name) + attr->data.data_len;
	}

	cinfo->stat_obj_rx++;
	cinfo->stat_obj_attr_byte_rx += attr_len;
	cinfo->stat_obj_hdr_byte_rx += hdr_len;
	cinfo->stat_obj_data_byte_rx += obj_len;
	cinfo->stat_obj_total_byte_rx += obj_len + hdr_len + attr_len;

	if (obj_len == 0 && attr_len == 0)
	{
		(*dev->cb.search_done_cb) (dev->hcookie);
		odisk_release_obj(obj);
	} else {
		/* XXX put it into the object ring */
		obj->dev_cookie = (intptr_t)dev;
		err = ring_enq(dev->obj_ring, obj);
		if (err != 0)
		{
		  g_error("Too many objects coming at us at once, probably "
			  "because you have connected to "
			  "more than %d servers for the "
			  "first time. Either fix the code or increase "
			  "OBJ_RING_SIZE as a terrible, dirty hack.",
			  OBJ_RING_SIZE / DEFAULT_QUEUE_LEN);
		}
	}
}

static const struct blast_channel_client_operations ops = {
	.send_object = recv_object,
};
const struct blast_channel_client_operations *hstub_blast_ops = &ops;


/* This is called when we want to write the credit count. */
void hstub_send_credits(sdevice_state_t *dev)
{
	conn_info_t *cinfo = &dev->con_data;
	credit_x credit;
	mrpc_status_t rc;

	pthread_mutex_lock(&cinfo->mutex);
	credit.credit_offset = cinfo->objects_consumed;
	cinfo->objects_consumed = 0;
	pthread_mutex_unlock(&cinfo->mutex);

	if (credit.credit_offset == 0)
		return;

	//	g_debug("sending %d credit_offset to %d", credit.credit_offset, cinfo->ipv4addr);

	rc = blast_channel_offset_credit(cinfo->blast_conn, &credit);
}

