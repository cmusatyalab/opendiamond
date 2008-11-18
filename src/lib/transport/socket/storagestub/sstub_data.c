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

#include "blast_channel_server.h"

int
sstub_queued_objects(void *cookie)
{
	cstate_t       *cstate = (cstate_t *) cookie;
	int             count;

	count = ring_count(cstate->partial_obj_ring);
	count += ring_count(cstate->complete_obj_ring);
	return (count);
}

static int
drop_attributes(cstate_t * cstate)
{
	unsigned int    rv;
	int             tx_count;
	if ((cstate->attr_policy == NW_ATTR_POLICY_PROPORTIONAL) ||
	    (cstate->attr_policy == NW_ATTR_POLICY_FIXED)) {
		rv = random();
		if (rv > cstate->attr_threshold) {
			return (1);
		} else {
			return (0);
		}
	} else if (cstate->attr_policy == NW_ATTR_POLICY_QUEUE) {
		tx_count = sstub_queued_objects(cstate);
		if ((tx_count > DESIRED_MAX_TX_THRESH) &&
		    (cstate->cc_credits >= DESIRED_CREDIT_THRESH)) {
			return (1);
		} else {
			return (0);
		}
	}
	return (0);
}

static float
prop_get_tx_ratio(cstate_t * cstate)
{
	float           ratio;
	int             count;

	count = sstub_queued_objects(cstate);
	ratio = ((float) count) / (float) DESIRED_MAX_TX_QUEUE;
	if (ratio > 1.0) {
		ratio = 1.0;
	} else if (ratio < 0) {
		ratio = 0.0;
	}
	return (ratio);
}

static float
prop_get_rx_ratio(cstate_t * cstate)
{
	float           ratio;

	ratio = ((float) cstate->cc_credits) / (float) DESIRED_MAX_CREDITS;
	if (ratio > 1.0) {
		ratio = 1.0;
	} else if (ratio < 0) {
		ratio = 0.0;
	}
	return (ratio);
}


static void
update_attr_policy(cstate_t * cstate)
{
	float           tx_ratio;
	float           rx_ratio;

	/*
	 * we only do updates for the proportional scheduling today.
	 */
	if (cstate->attr_policy == NW_ATTR_POLICY_PROPORTIONAL) {
		tx_ratio = prop_get_tx_ratio(cstate);
		rx_ratio = prop_get_rx_ratio(cstate);
		/*
		 * we use the min to set the threshold 
		 */
		if (rx_ratio < tx_ratio) {
			cstate->attr_threshold = rx_ratio * RAND_MAX;
			cstate->attr_ratio = (int) (rx_ratio * 100.0);
		} else {
			cstate->attr_threshold = tx_ratio * RAND_MAX;
			cstate->attr_ratio = (int) (tx_ratio * 100.0);
		}

	} else if (cstate->attr_policy == NW_ATTR_POLICY_FIXED) {
		if (cstate->attr_ratio == 100) {
			cstate->attr_threshold = RAND_MAX;
		} else {
			cstate->attr_threshold =
			    (RAND_MAX / 100) * cstate->attr_ratio;
		}
	}
	return;
}

int sstub_get_attributes(obj_attr_t *obj_attr, GArray *output_set,
			 attribute_x **result_val, unsigned int *result_len,
			 int drop_attrs)
{
	struct acookie *cookie;
	attribute_x *attrs;
	unsigned int i, n;
	char *name;
	void *data;
	size_t len;
	int err;

	/* how many attributes could we be sending? (worst case) */
	err = obj_first_attr(obj_attr, NULL, NULL, NULL, NULL, &cookie,
			     drop_attrs);
	for (n = 0; err == 0; n++)
		err = obj_next_attr(obj_attr, NULL, NULL, NULL, NULL, &cookie,
				    drop_attrs);

	attrs = malloc(n * sizeof(attribute_x));
	if (n != 0 && attrs == NULL)
		return -1;

	err = obj_first_attr(obj_attr, &name, &len, (unsigned char **)&data,
			     NULL, &cookie, drop_attrs);
	for (n = 0; err == 0;)
	{
		GQuark attrq = g_quark_from_string(name);

		for (i = 0; i < output_set->len; i++) {
			if (attrq == g_array_index(output_set, GQuark, i))
			{
			    attrs[n].name = name;
			    attrs[n].data.data_len = len;
			    attrs[n].data.data_val = data;
			    n++;
			}
		}
		err = obj_next_attr(obj_attr, &name, &len,
				    (unsigned char **)&data, NULL, &cookie,
				    drop_attrs);
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
	int		drop_attrs;
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

	/* periodically we want to update our send policy if we are dynamic. */
	if ((cstate->stats_objs_tx & 0xF) == 0)
		update_attr_policy(cstate);

	/* Decide if we are going to send the attributes on this object. */
	drop_attrs = drop_attributes(cstate);

	object.search_id = cstate->search_id;
	object.object.object_id_x_len = obj->data_len;
	object.object.object_id_x_val = obj->data;

	tx_hdr_bytes = sizeof(object_x);
	tx_data_bytes = obj->data_len;

	err = sstub_get_attributes(&obj->attr_info, cstate->thumbnail_set,
				   &object.attrs.attrs_val,
				   &object.attrs.attrs_len, drop_attrs);
	if (err) {
	    free_object_x(&object, FALSE);
	    return;
	}

	for (n = 0; n < object.attrs.attrs_len; n++)
	{
		tx_hdr_bytes += sizeof(attribute_x);
		tx_data_bytes += object.attrs.attrs_val[n].data.data_len;
	}
	free_object_x(&object, FALSE);

	rc = blast_channel_send_object(cstate->blast_conn, &object);
	assert(rc == MINIRPC_OK);

	cstate->stats_objs_tx++;
	cstate->stats_objs_hdr_bytes_tx += tx_hdr_bytes;
	cstate->stats_objs_data_bytes_tx += tx_data_bytes;
	cstate->stats_objs_total_bytes_tx += tx_hdr_bytes + tx_data_bytes;

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

