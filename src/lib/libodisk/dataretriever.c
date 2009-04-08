#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <curl/curl.h>

#include "diamond_types.h"
#include "sys_attr.h"
#include "lib_odisk.h"
#include "odisk_priv.h"
#include "lib_log.h"

static long odisk_curl_debug = 0;

struct fetch_state {
    CURL *handle;
    obj_attr_t attr;
    void *data;
    size_t len;
    size_t offset;
};

/* extract OpenDiamond attributes from HTTP response headers */
static size_t
obj_header_cb(void *ptr, size_t size, size_t nmemb, void *arg)
{
    struct fetch_state *s = arg;
    size_t len, buflen = size * nmemb;
    char *value;

    /* smallest possible valid attribute header 'x-attr-x:'? */
    if (buflen < 9 || g_str_has_prefix(ptr, "x-attr-"))
	return buflen;

    /* strip the prefix and trailing \r\n */
    ptr = (char *)ptr + 7; len = buflen - 9;
    *((char *)ptr + len) = '\0';

    /* find the end of the key/start of value */
    value = g_strstr_len(ptr, len, ":");
    if (!value) return buflen;
    *(value++) = '\0';

    /* skip leading whitespace */
    value = g_strchug(value);
    len = strlen(value);

    /* if we found a usable value, add the attribute to the object */
    if (len)
	obj_write_attr(&s->attr, ptr, len+1, (void *)value);

    return buflen;
}

static size_t
obj_content_cb(void *ptr, size_t size, size_t nmemb, void *arg)
{
    struct fetch_state *s = arg;
    size_t buflen = size * nmemb;
    double len = -1;

    if (!buflen) return 0;

    /* if we don't have data yet, get value of content-length header */
    if (!s->data)
	curl_easy_getinfo(s->handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len);

    /* make sure we have enough space allocated */
    if (s->len < s->offset + buflen) {
	s->len = (len == -1) ? s->offset + buflen : (size_t)len;
	s->data = realloc(s->data, s->len);
	if (!s->data) return 0;
    }

    /* copy received data to the destination buffer */
    memcpy((char *)s->data + s->offset, ptr, buflen);
    s->offset += buflen;
    return buflen;
}

obj_data_t *dataretriever_fetch_object(const char *name)
{
    static CURL *handle = NULL;
    struct fetch_state s = { .handle = NULL };
    obj_data_t *obj;
    CURLcode cc;

    if (!handle) {
	handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_VERBOSE, odisk_curl_debug);
	curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "OpenDiamond-adiskd/1.0");
	curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 1L);
    }

    s.handle = handle;

    //curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(handle, CURLOPT_URL, name);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, obj_header_cb);
    curl_easy_setopt(handle, CURLOPT_WRITEHEADER, &s);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, obj_content_cb);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &s);

    cc = curl_easy_perform(handle);
    if (cc) {
	const char *url, *err = curl_easy_strerror(cc);
	obj_adata_t *cur, *next;

	curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &url);
	log_message(LOGT_DISK, LOGL_ERR, "cURL error: %s (%s)\n", err, url);

	/* destroy stuff that got allocated while receiving the object */
	for (cur = s.attr.attr_dlist; cur; cur = next)
	{
	    next = cur->adata_next;
	    free(cur->adata_data);
	    free(cur);
	}
	free(s.data);
	return NULL;
    }

    obj = calloc(1, sizeof(*obj));
    assert(obj);
    obj->attr_info = s.attr;
    obj_write_attr(&obj->attr_info, OBJ_DATA, s.len, s.data);
    free(s.data);
    return obj;
}

