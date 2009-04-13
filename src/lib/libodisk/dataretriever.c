/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2009 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <libsoup/soup.h>

#include "diamond_types.h"
#include "sys_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "lib_log.h"

/*************************************/
/* Initialize data retriever globals */

static SoupURI *collection_base_uri;
static SoupSession *scopelist_sess;
static SoupSession *object_session;
static long odisk_http_debug;

#define DBG(msg, ...) do { \
    if (odisk_http_debug) \
	fprintf(stderr, "%s: " msg, __FUNCTION__, ## __VA_ARGS__); \
} while(0)

/* Called when a request has completed */
static void request_unqueued(SoupSession *session, SoupMessage *msg,
			     SoupSocket *socket, gpointer user_data)
{
    GMarkupParseContext *context =
	g_object_get_data(G_OBJECT(msg), "parse-context");
    DBG("msg %p context %p\n", msg, context);
    if (context) {
	g_markup_parse_context_end_parse(context, NULL);
	g_markup_parse_context_free(context);
	g_object_set_data(G_OBJECT(msg), "parse-context", NULL);
    }
}

void dataretriever_init(const char *base_uri)
{
    g_type_init();

    collection_base_uri = soup_uri_new(base_uri);

    scopelist_sess = soup_session_sync_new_with_options(
	SOUP_SESSION_USER_AGENT,	"OpenDiamond-adiskd ",
	SOUP_SESSION_MAX_CONNS,		 1,
	SOUP_SESSION_MAX_CONNS_PER_HOST, 1,
	NULL);
    g_signal_connect(scopelist_sess, "request-unqueued",
		     G_CALLBACK(request_unqueued), NULL);

    object_session = soup_session_sync_new_with_options(
	SOUP_SESSION_USER_AGENT,	"OpenDiamond-adiskd ",
	SOUP_SESSION_MAX_CONNS,		64,
	SOUP_SESSION_MAX_CONNS_PER_HOST, 8,
	NULL);

    if (odisk_http_debug) {
	SoupLogger *logger = soup_logger_new(SOUP_LOGGER_LOG_HEADERS, -1);
	soup_session_add_feature(scopelist_sess, SOUP_SESSION_FEATURE(logger));
	soup_session_add_feature(object_session, SOUP_SESSION_FEATURE(logger));
	g_object_unref(logger);
    }
}


/*******************************************/
/* Parse scopelist and extract object URLs */

struct queue_msg {
    enum { URI, COUNT, PAUSED, DONE } type;
    union {
	SoupURI *uri;
	int64_t count;
	SoupMessage *msg;
    } u;
};

static void free_queue_msg(gpointer data)
{
    struct queue_msg *qmsg = data;
    if (qmsg->type == URI)
	soup_uri_free(qmsg->u.uri);
    else if (qmsg->type == PAUSED)
	soup_session_cancel_message(scopelist_sess, qmsg->u.msg,
				    SOUP_STATUS_CANCELLED);
    g_slice_free(struct queue_msg, qmsg);
}

struct fetch_state {
    SoupURI *scope_uri;
    GAsyncQueue *queue;
};

static void free_fetch_state(gpointer data)
{
    struct fetch_state *state = data;
    struct queue_msg *qmsg = g_slice_new0(struct queue_msg);

    DBG("state %p\n", state);
    qmsg->type = DONE;
    g_async_queue_push(state->queue, qmsg);
    g_async_queue_unref(state->queue);
    soup_uri_free(state->scope_uri);
    g_free(state);
}

static void start_element(GMarkupParseContext *ctx, const gchar *element_name,
			  const gchar **attr_names, const gchar **attr_values,
			  gpointer user_data, GError **err)
{
    struct fetch_state *state = user_data;
    const char *value = NULL;
    struct queue_msg *qmsg = NULL;

    if (strcmp(element_name, "object") == 0) {
	g_markup_collect_attributes(element_name, attr_names, attr_values, err,
	    G_MARKUP_COLLECT_STRING, "src", &value,
	    G_MARKUP_COLLECT_INVALID, NULL, NULL);
	if (!value) return;

	qmsg = g_slice_new0(struct queue_msg);
	qmsg->type = URI;
	qmsg->u.uri = soup_uri_new_with_base(state->scope_uri, value);
    }

    else if (strcmp(element_name, "count") == 0) {
	g_markup_collect_attributes(element_name, attr_names, attr_values, err,
	    G_MARKUP_COLLECT_STRING, "adjust", &value,
	    G_MARKUP_COLLECT_INVALID, NULL, NULL);
	if (!value) return;

	qmsg = g_slice_new0(struct queue_msg);
	qmsg->type = COUNT;
	qmsg->u.count = (int64_t)strtoll(value, NULL, 10);
    }

    else if (strcmp(element_name, "objectlist") == 0) {
	g_markup_collect_attributes(element_name, attr_names, attr_values, err,
	    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "count", &value,
	    G_MARKUP_COLLECT_INVALID, NULL, NULL);
	if (!value) return;

	qmsg = g_slice_new0(struct queue_msg);
	qmsg->type = COUNT;
	qmsg->u.count = (int64_t)strtoll(value, NULL, 10);
    }
    if (qmsg)
	g_async_queue_push(state->queue, qmsg);
}

static const GMarkupParser scopelist_parser = {
    .start_element = start_element,
};

/*************************************************************************/
/* Fetch scopelist from the data retriever and pass it to the XML parser */

/* Called as the scope list is received from the data retriever */
static void scopelist_got_chunk(SoupMessage *msg, SoupBuffer *b, gpointer ud)
{
    GMarkupParseContext *context = ud;
    struct fetch_state *state = g_markup_parse_context_get_user_data(context);
    struct queue_msg *qmsg;

    if (!g_markup_parse_context_parse(context, b->data, b->length, NULL))
	soup_session_cancel_message(scopelist_sess, msg, SOUP_STATUS_MALFORMED);

    DBG("Pausing %p (qlen %d)\n", msg, g_async_queue_length(state->queue));
    soup_session_pause_message(scopelist_sess, msg);

    qmsg = g_slice_new0(struct queue_msg);
    qmsg->type = PAUSED;
    qmsg->u.msg = msg;
    g_async_queue_push(state->queue, qmsg);
}

static void dataretriever_fetch_scopelist(SoupURI *uri, GAsyncQueue *queue)
{
    GMarkupParseContext *context;
    SoupMessage *msg;
    struct fetch_state *state;

    state = g_new0(struct fetch_state, 1);
    state->scope_uri = uri;
    state->queue = g_async_queue_ref(queue);

    context = g_markup_parse_context_new(&scopelist_parser, 0,
					 state, free_fetch_state);

    msg = soup_message_new_from_uri("GET", uri);
    soup_message_body_set_accumulate(msg->response_body, FALSE);

    g_object_set_data(G_OBJECT(msg), "parse-context", context);
    g_signal_connect(msg, "got-chunk", G_CALLBACK(scopelist_got_chunk),context);

    soup_session_queue_message(scopelist_sess, msg, NULL, NULL);
}


/*********************/
/* Start/stop search */

void dataretriever_start_search(odisk_state_t *odisk)
{
    int i;

    /* make sure active scopelist fetches are aborted */
    dataretriever_stop_search(odisk);

    odisk->fetchers = odisk->num_gids;
    odisk->queue = g_async_queue_new_full(free_queue_msg);
    odisk->count = 0;

    for (i = 0; i < odisk->num_gids; i++) {
	char gid_string[40];
	SoupURI *uri;
	groupid_t gid = odisk->gid_list[i];

	snprintf(gid_string, 40,
		 "%02X%%3A%02X%%3A%02X%%3A%02X%%3A%02X%%3A%02X%%3A%02X%%3A%02X",
		 (uint32_t)((gid >> 56) & 0xff), (uint32_t)((gid >> 48) & 0xff),
		 (uint32_t)((gid >> 40) & 0xff), (uint32_t)((gid >> 32) & 0xff),
		 (uint32_t)((gid >> 24) & 0xff), (uint32_t)((gid >> 16) & 0xff),
		 (uint32_t)((gid >>  8) & 0xff), (uint32_t)(gid & 0xff));

	uri = soup_uri_new_with_base(collection_base_uri, gid_string);
	dataretriever_fetch_scopelist(uri, odisk->queue);
    }
}

void dataretriever_stop_search(odisk_state_t *odisk)
{
    if (!odisk->queue) return;

    /* Abort remaining transfers so they fail the request. This calls the
     * msg->finished callback. which destroys the XML parse context. That
     * in turn deallocates the fetch_state structure where we push a final
     * message into the async queue and unref the fetching side. Once the
     * consumer sees the final message it will unref the other end of the
     * queue and clear odisk->queue. */
    soup_session_abort(scopelist_sess);

    /* We'll have the consumer destroy the queue before the final message
     * from the scopelist retrieving thread arrives. */
    //odisk->fetchers = 0;

    /* wait for the queue to disappear */
    while(odisk->queue) {
	DBG("Waiting for queue to drain (qlen %d)\n",
	    g_async_queue_length(odisk->queue));
	sleep(1);
    }
}

char *dataretriever_next_object_uri(odisk_state_t *odisk)
{
    struct queue_msg *qmsg;
    char *uri_string = NULL;

    while (odisk->fetchers && !uri_string)
    {
	qmsg = g_async_queue_pop(odisk->queue);

	if (qmsg->type == URI) {
	    uri_string = soup_uri_to_string(qmsg->u.uri, FALSE);
	    soup_uri_free(qmsg->u.uri);
	}
	else if (qmsg->type == COUNT) {
	    odisk->count += qmsg->u.count;
	}
	else if (qmsg->type == PAUSED) {
	    DBG("Unpausing %p\n", qmsg->u.msg);
	    soup_session_unpause_message(scopelist_sess, qmsg->u.msg);
	}
	else if (qmsg->type == DONE) {
	    odisk->fetchers--;
	    DBG("%d fetchers remaining\n", odisk->fetchers);
	}
	g_slice_free(struct queue_msg, qmsg);
    }
    if (!uri_string && odisk->queue) {
	g_async_queue_unref(odisk->queue);
	odisk->queue = NULL;
    }
    return uri_string;
}


/**************************************************/
/* Fetch Diamond objects from the data retriever. */

/* extract OpenDiamond attributes from HTTP response headers */
static void get_attribute(const char *name, const char *value, gpointer udata)
{
    obj_attr_t *attrs = (obj_attr_t *)udata;

    if (g_str_has_prefix(name, "x-attr-"))
	obj_write_attr(attrs, &name[7], strlen(value) + 1, (void *)value);
}

obj_data_t *dataretriever_fetch_object(const char *uri_string)
{
    SoupMessage *msg;
    SoupBuffer *body;
    obj_data_t *obj;

    msg = soup_message_new("GET", uri_string);
    soup_session_send_message(object_session, msg);

    if (!SOUP_STATUS_IS_SUCCESSFUL(msg->status_code))
    {
	log_message(LOGT_DISK, LOGL_ERR, "data fetch: %d %s (%s)\n",
		    msg->status_code, msg->reason_phrase, uri_string);
	return NULL;
    }

    obj = calloc(1, sizeof(*obj));
    assert(obj);

    soup_message_headers_foreach(msg->response_headers,
				 get_attribute, &obj->attr_info);
    body = soup_message_body_flatten(msg->response_body);
    obj_write_attr(&obj->attr_info, OBJ_DATA, body->length, (void *)body->data);
    soup_buffer_free(body);

    g_object_unref(msg);
    return obj;
}

