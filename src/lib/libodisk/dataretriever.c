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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <strings.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <libsoup/soup.h>

#include "diamond_types.h"
#include "sys_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "lib_log.h"
#include "scope_priv.h"


/* compatibility wrappers for libsoup-2.2 */
#ifdef HAVE_LIBSOUP22
#define SoupURI SoupUri
#define soup_session_cancel_message(session, message, status) \
	soup_session_cancel_message(session, message)
#define soup_session_pause_message(session, message) \
	soup_message_io_pause(message)
#define soup_session_unpause_message(session, message) \
	soup_message_io_unpause(message)
#define soup_message_set_accumulate(msg, yesno) do { \
	guint flags = soup_message_get_flags(msg); \
	if (yesno) flags &= ~SOUP_MESSAGE_OVERWRITE_CHUNKS; \
	else       flags |= SOUP_MESSAGE_OVERWRITE_CHUNKS; \
	soup_message_set_flags(msg, flags); \
    } while(0)
#define soup_message_headers_foreach(headers, function, user_data) \
	soup_message_foreach_header(headers, (GHFunc)function, user_data)
#define soup_message_body_flatten(body) ((void *)0)
#define soup_buffer_free(buf)
#define SoupBuffer void

#define scopelist_got_chunk(message, buffer, user_data) \
	scopelist_got_chunk(message, user_data)
#define RESPONSE_BUFFER ((void *)(msg)->response.body)
#define RESPONSE_LENGTH ((msg)->response.length)

#else /* HAVE_LIBSOUP24 */
#define soup_message_set_accumulate(msg, yesno) \
	soup_message_body_set_accumulate((msg)->response_body, yesno)
#define RESPONSE_BUFFER ((void *)(buf)->data)
#define RESPONSE_LENGTH ((buf)->length)
#endif

#ifdef HAVE_GLIB2_OLD
#define g_async_queue_new_full(freeitem) g_async_queue_new()

#define G_MARKUP_COLLECT_INVALID  0
#define G_MARKUP_COLLECT_STRING   0
#define G_MARKUP_COLLECT_OPTIONAL 0
/* extremely minimal implementation as we only need to find one string value */
gboolean g_markup_collect_attributes (const gchar *element_name,
	const gchar **attribute_names, const gchar **attribute_values,
	GError **error, int type, const gchar *attr, const gchar **value, ...)
{
    for (; *attribute_names; attribute_names++, attribute_values++)
	if (strcmp(*attribute_names, attr) == 0)
	    break;
    *value = *attribute_values;
    return (*attribute_names != NULL);
}
#endif


/*************************************/
/* Initialize data retriever globals */

static SoupURI *BASE_URI;
static SoupSession *scopelist_sess;
static SoupSession *object_session;
static long odisk_http_debug;

#define DBG(msg, ...) do { \
    if (odisk_http_debug) \
	fprintf(stderr, "%s: " msg, __FUNCTION__, ## __VA_ARGS__); \
} while(0)

struct fetch_state {
    SoupURI *scope_uri;
    GAsyncQueue *queue;
    GMarkupParseContext *context;
    GError *error;
};

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

/* Called when a request has completed */
static void request_unqueued(SoupSession *session, SoupMessage *msg,
			     SoupSocket *socket, gpointer user_data)
{
    struct fetch_state *state;
    struct queue_msg *qmsg;

    state = g_object_get_data(G_OBJECT(msg), "parse-context");
    if (!state) return;
    g_object_set_data(G_OBJECT(msg), "parse-context", NULL);

    if (!state->error)
	g_markup_parse_context_end_parse(state->context, &state->error);

    qmsg = g_slice_new0(struct queue_msg);
    qmsg->type = DONE;
    g_async_queue_push(state->queue, qmsg);

    /* release resources */
    g_async_queue_unref(state->queue);
    soup_uri_free(state->scope_uri);
    g_markup_parse_context_free(state->context);
    if (state->error) g_error_free(state->error);
    g_free(state);
}

void dataretriever_init(const char *base_url)
{
    g_type_init();

    BASE_URI = soup_uri_new(base_url);

    scopelist_sess = soup_session_sync_new_with_options(
	SOUP_SESSION_MAX_CONNS,		 1,
	SOUP_SESSION_MAX_CONNS_PER_HOST, 1,
	NULL);
    g_signal_connect(scopelist_sess, "request-unqueued",
		     G_CALLBACK(request_unqueued), NULL);

    object_session = soup_session_sync_new_with_options(
	SOUP_SESSION_MAX_CONNS,		64,
	SOUP_SESSION_MAX_CONNS_PER_HOST, 8,
	NULL);
}


/*******************************************/
/* Parse scopelist and extract object URLs */
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
static void scopelist_got_chunk(SoupMessage *msg, SoupBuffer *buf, gpointer ud)
{
    struct fetch_state *state = ud;
    struct queue_msg *qmsg;

    if (state->error) return;
    if (!g_markup_parse_context_parse(state->context,
				      RESPONSE_BUFFER, RESPONSE_LENGTH,
				      &state->error))
    {
	soup_session_cancel_message(scopelist_sess, msg, SOUP_STATUS_MALFORMED);
	return;
    }

    DBG("Pausing %p (qlen %d)\n", msg, g_async_queue_length(state->queue));
    soup_session_pause_message(scopelist_sess, msg);

    qmsg = g_slice_new0(struct queue_msg);
    qmsg->type = PAUSED;
    qmsg->u.msg = msg;
    g_async_queue_push(state->queue, qmsg);
}

static void dataretriever_fetch_scopelist(SoupURI *uri, GAsyncQueue *queue,
					  unsigned int search_id)
{
    struct fetch_state *state;
    SoupMessage *msg;
    char searchid[11];

    state = g_new0(struct fetch_state, 1);
    state->scope_uri = uri;
    state->queue = g_async_queue_ref(queue);
    state->context =
	g_markup_parse_context_new(&scopelist_parser, 0, state, NULL);

    msg = soup_message_new_from_uri("GET", uri);
    soup_message_set_accumulate(msg, FALSE);

    snprintf(searchid, 11, "%u", search_id);
    soup_message_headers_replace(msg->request_headers, "x-searchid", searchid);

    g_object_set_data(G_OBJECT(msg), "parse-context", state);
    g_signal_connect(msg, "got-chunk", G_CALLBACK(scopelist_got_chunk), state);

    soup_session_queue_message(scopelist_sess, msg, NULL, NULL);
}


/*********************/
/* Start/stop search */
static void dispatch_fetchers(gpointer data, gpointer user_data)
{
    odisk_state_t *odisk = user_data;
    struct scopecookie *cookie = data;
    SoupURI *uri;
    gchar **scope;
    unsigned int i;

    scope = g_strsplit(cookie->scopedata, "\n", 0);
    for (i = 0; scope[i]; i++)
    {
	if (*scope[i] == '\0') continue;

	uri = soup_uri_new_with_base(BASE_URI, scope[i]);

	odisk->fetchers++;
	dataretriever_fetch_scopelist(uri, odisk->queue, odisk->search_id);
    }
    g_strfreev(scope);
}

void dataretriever_start_search(odisk_state_t *odisk)
{
    /* make sure active scopelist fetches are aborted */
    dataretriever_stop_search(odisk);

    odisk->fetchers = odisk->count = 0;
    odisk->queue = g_async_queue_new_full(free_queue_msg);

    odisk->fetchers++;
    g_ptr_array_foreach(odisk->scope, dispatch_fetchers, odisk);
    odisk->fetchers--;
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
    SoupBuffer *buf;
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

    buf = soup_message_body_flatten(msg->response_body);
    obj_write_attr(&obj->attr_info, OBJ_DATA, RESPONSE_LENGTH, RESPONSE_BUFFER);
    soup_buffer_free(buf);

    g_object_unref(msg);
    return obj;
}

