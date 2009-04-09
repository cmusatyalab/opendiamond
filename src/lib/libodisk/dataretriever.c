#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "diamond_types.h"
#include "sys_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "lib_log.h"

static long odisk_http_debug = 0;

/* extract OpenDiamond attributes from HTTP response headers */
static void get_attribute(const char *name, const char *value, gpointer udata)
{
    obj_attr_t *attrs = (obj_attr_t *)udata;

    if (g_str_has_prefix(name, "x-attr-"))
	obj_write_attr(attrs, &name[7], strlen(value) + 1, (void *)value);
}

obj_data_t *dataretriever_fetch_object(SoupURI *uri)
{
    static SoupSession *session = NULL;
    SoupMessage *msg;
    SoupBuffer *body;
    guint status;
    obj_data_t *obj;

    if (!session) {
	g_type_init();
	session = soup_session_sync_new_with_options(
	    SOUP_SESSION_USER_AGENT, "OpenDiamond-adiskd ",
	    NULL);

	if (odisk_http_debug) {
	    SoupLogger *logger = soup_logger_new(SOUP_LOGGER_LOG_HEADERS, -1);
	    soup_session_add_feature(session, SOUP_SESSION_FEATURE(logger));
	    g_object_unref(logger);
	}
    }

    msg = soup_message_new_from_uri("GET", uri);
    soup_uri_free(uri);

    status = soup_session_send_message(session, msg);
    if (!SOUP_STATUS_IS_SUCCESSFUL(status))
    {
	char *uri_string = soup_uri_to_string(soup_message_get_uri(msg), FALSE);

	log_message(LOGT_DISK, LOGL_ERR, "data fetch: %d %s (%s)\n",
		    msg->status_code, msg->reason_phrase, uri_string);

	g_free(uri_string);
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

