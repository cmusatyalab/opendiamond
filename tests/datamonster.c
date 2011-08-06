/*
 *  The OpenDiamond Platform for Interactive Search
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include <glib/gstdio.h>

static gboolean presort;
static gboolean sort;
static gboolean decompress;
static gchar *dataroot;
static gchar **idxfiles;

static GOptionEntry options[] = {
    { "presort", 'p', 0, G_OPTION_ARG_NONE, &presort,
	"Sort index files by name", NULL },
    { "sort", 's', 0, G_OPTION_ARG_NONE, &sort,
	"Sort index files by inode", NULL },
#ifdef HAVE_JPEGLIB_H
    { "jpeg", 'j', 0, G_OPTION_ARG_NONE, &decompress,
	"Decompress jpeg files", NULL },
#endif
    { "dataroot", 'd', 0, G_OPTION_ARG_FILENAME, &dataroot,
	"Directory containing files", "DATAROOT" },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &idxfiles,
	"Files containing filenames to read", "INDEXFILE" },
    { .long_name = NULL, },
};

struct elem {
    gchar *file;
    ino_t ino;
};

static int cmp_by_name(gconstpointer a, gconstpointer b)
{
    const struct elem *ea = a;
    const struct elem *eb = b;
    return strcmp(ea->file, eb->file);
}

static int cmp_by_ino(gconstpointer a, gconstpointer b)
{
    const struct elem *ea = a;
    const struct elem *eb = b;
    if (ea->ino < eb->ino) return -1;
    if (ea->ino == eb->ino) return 0;
    return 1;
}

static GArray *read_index(const gchar *idxfile)
{
    gchar *gididx, **files;
    GArray *array;
    unsigned int i;
    struct elem elem = { .ino = 0 };
    int rc;

    rc = g_file_get_contents(idxfile, &gididx, NULL, NULL);
    assert(rc);
    
    files = g_strsplit(gididx, "\n", -1);
    g_free(gididx);

    for (i = 0; files[i]; i++) /* count # of entries */;

    array = g_array_sized_new(FALSE, TRUE, sizeof(struct elem), i);

    for (i = 0; files[i]; i++) {
	if (!*files[i]) {
	    g_free(files[i]);
	    continue;
	}
	elem.file = files[i];
	g_array_append_val(array, elem);
    }
    g_free(files);
    return array;
}

static void collect_inos(GArray *array)
{
    struct elem *elem;
    struct stat buf;
    unsigned int i;

    for (i = 0; i < array->len; i++)
    {
	elem = &g_array_index(array, struct elem, i);

	if (g_stat(elem->file, &buf)) {
	    fprintf(stderr, "Failed to stat \"%s\"\n", elem->file);
	    continue;
	}
	elem->ino = buf.st_ino;
    }
}

#ifdef HAVE_JPEGLIB_H
#include <jpeglib.h>

/* helper functions for the jpeg decompressor */
static void init_source(j_decompress_ptr cinfo)
{
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
    return TRUE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    if (num_bytes <= 0) return;
    cinfo->src->next_input_byte += (size_t)num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t)num_bytes;
}

static void term_source(j_decompress_ptr cinfo)
{
}

static void decompress_jpeg(const gchar *buf, gsize len)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_source_mgr source;
    JSAMPROW line;

    if (len < 2 || buf[0] != (gchar)0xff || buf[1] != (gchar)0xd8)
	return;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    cinfo.src = &source;
    cinfo.src->next_input_byte = (const JOCTET *)buf;
    cinfo.src->bytes_in_buffer = len;
    cinfo.src->init_source = init_source;
    cinfo.src->fill_input_buffer = fill_input_buffer;
    cinfo.src->skip_input_data = skip_input_data;
    cinfo.src->term_source = term_source;

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    line = g_new(JSAMPLE, cinfo.output_width * cinfo.output_components);

    while(cinfo.output_scanline < cinfo.output_height)
	jpeg_read_scanlines(&cinfo, &line, 1);

    g_free(line);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}
#else
static void decompress_jpeg(const gchar *buf, gsize len)
{
}
#endif

int main(int argc, char **argv)
{
    GOptionContext *context;
    GError *err = NULL;
    GArray *files;
    gchar *buf;
    gsize len;
    unsigned int i;
    uint64_t nbytes = 0;
    GTimer *timer;
    gdouble elapsed;

    context = g_option_context_new("- read lots of data");
    g_option_context_add_main_entries(context, options, NULL);
    g_option_context_parse(context, &argc, &argv, &err);

    if (err) {
	fprintf(stderr, "%s\n", err->message);
	g_error_free(err);
	exit(0);
    }

    if (!idxfiles || idxfiles[0] == NULL) {
	fprintf(stderr, "No index file specified\n");
	exit(0);
    }

    timer = g_timer_new();
    files = read_index(idxfiles[0]);
    printf("Read index at %.3f sec\n", g_timer_elapsed(timer, NULL));

    if (presort) {
	g_array_sort(files, cmp_by_name);
	printf("Presorted index at %.3f sec\n", g_timer_elapsed(timer, NULL));
    }

    g_chdir(dataroot);

    if (sort) {
	collect_inos(files);
	printf("Collected inode numbers at %.3f sec\n",
	       g_timer_elapsed(timer, NULL));

	g_array_sort(files, cmp_by_ino);
	printf("Sorted index at %.3f sec\n", g_timer_elapsed(timer, NULL));
    }

    fflush(stdout);

    g_timer_start(timer);
    for (i = 0; i < files->len; i++) {
	g_file_get_contents(g_array_index(files, struct elem, i).file,
			    &buf, &len, NULL);
	if (decompress) 
	    decompress_jpeg(buf, len);

	g_free(buf);
	nbytes += len;
    }
    elapsed = g_timer_elapsed(timer, NULL);

    printf("Elapsed time: %.3f sec\n", elapsed);
    printf("Objects read: %u (%.3f obj/s)\n", i, (double)i / elapsed);
    printf("Bytes read: %llu (%.3lf bps)\n", nbytes, (double)nbytes / elapsed);

    for (i = 0; i < files->len; i++)
	g_free(g_array_index(files, struct elem, i).file);
    g_array_free(files, TRUE);
    g_strfreev(idxfiles);
    g_free(dataroot);
    g_timer_destroy(timer);
    g_option_context_free(context);
    return 0;
}

