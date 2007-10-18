/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2006 Intel Corporation
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>

#include <glib.h>

#include "sig_calc.h"
#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_odisk.h"
#include "lib_dctl.h"
#include "lib_log.h"
#include "lib_filterexec.h"
#include "lib_dconfig.h"
#include "fexec_history.h"

static gboolean sig_equal_fn(gconstpointer a, gconstpointer b) {
	gboolean match = FALSE;
	
	sig_val_t *sig1 = (sig_val_t *) a;
	sig_val_t *sig2 = (sig_val_t *) b;
	if (sig_match(sig1, sig2))
		match = TRUE;
		
	return (match);
}

static guint sig_hash_fn(gconstpointer key) {
	sig_val_t *sig = (sig_val_t *) key;
	return (guint) sig_hash(sig);
}

static void history_destroy_fn(gpointer data)
{
	filter_history_t *fh = (filter_history_t *)data;
	g_free(fh);
}

GHashTable *get_filter_history()
{
	char fname[PATH_MAX];
	char *path;
	FILE *fp;
	int cnt;
	GHashTable *histories = NULL;
	
	histories = g_hash_table_new_full(sig_hash_fn, sig_equal_fn, 
										NULL, history_destroy_fn);
	assert(histories != NULL);

	path = dconf_get_filter_cachedir();
	snprintf(fname, PATH_MAX, "%s/filter_history", path);
	free(path);

	fp = fopen(fname, "r");
	if (fp != NULL) {
		while (1) {
			filter_history_t *fh = g_malloc(sizeof(filter_history_t));
			char *sigstr;

			cnt = fscanf(fp, "%s %u %u %u %u %u \n", 
				sigstr, &fh->executions, &fh->search_objects,
				&fh->filter_objects, &fh->drop_objects, 
				&fh->last_run);
			if (cnt != 6) {
				break;
			}
			string_to_sig(sigstr, &fh->filter_sig);
			g_hash_table_insert(histories, &fh->filter_sig, fh);
		}
		fclose(fp);
	}
	
	return histories;
}

static int filter_history_cmp(const void* arg1, const void* arg2)
{
	const filter_history_t *h1 = arg1;
	const filter_history_t *h2 = arg2;
	return(h2->executions - h1->executions);
}

static void filter_history_iterator(gpointer key, gpointer value, gpointer user_data) {
	filter_history_t *new_entry = (filter_history_t *)value;
	filter_history_list_t *list = (filter_history_list_t *) user_data;
	
	filter_history_t *dest = &list->entries[list->num_entries++];
	memcpy(dest, new_entry, sizeof(filter_history_t));
}


static filter_history_list_t *get_history_by_frequency(GHashTable *histories) {
	
	filter_history_list_t *history_list = malloc(sizeof(filter_history_list_t));
	history_list->num_entries = g_hash_table_size(histories);
	history_list->entries = malloc(sizeof(filter_history_t) * 
									history_list->num_entries);
	assert(history_list->entries != NULL);

	g_hash_table_foreach(histories, filter_history_iterator, history_list);
	qsort(history_list->entries, history_list->num_entries, 
			sizeof(filter_history_t), filter_history_cmp);
			
	return history_list;
}

static void filter_history_writer(gpointer key, gpointer value, gpointer user_data) {

	char *sigstr;
	FILE *fp = (FILE *) user_data;
	filter_history_t *fh = (filter_history_t *)value;

	sigstr = sig_string(&fh->filter_sig);
	fprintf(fp, "%s %u %u %u %u %u \n", 
		sigstr, fh->executions, fh->search_objects,
		fh->filter_objects, fh->drop_objects, 
		fh->last_run);	
}

void write_filter_history(GHashTable *histories) {
	char fname[PATH_MAX];
	char *path;
	FILE *fp;

	path = dconf_get_filter_cachedir();
	snprintf(fname, PATH_MAX, "%s/filter_history", path);

	fp = fopen(fname, "w+");
	if (fp != NULL) {
		g_hash_table_foreach(histories, filter_history_writer, fp);
		fclose(fp);
	}
	
	return;
}

void update_filter_history(GHashTable *histories, gboolean remove)
{
	DIR *dir;
	char *path = dconf_get_filter_cachedir();
	char fname[PATH_MAX];
	char sig[PATH_MAX];
	unsigned int  obj, called, drop;
	struct dirent *cur_ent;
	filter_history_t *fh;
	sig_val_t sigval;
	FILE *fp;

	dir = opendir(path);

	while ((cur_ent = readdir(dir)) != NULL) {

		if (cur_ent->d_type != DT_REG) {
			continue;
		}
		if (strstr(cur_ent->d_name, "results.") != cur_ent->d_name) {
			continue;
		}
		snprintf(fname, PATH_MAX, "%s/%s", path, cur_ent->d_name);
		fp = fopen(fname, "r");
		if (fp == NULL) {
			continue;
		}

		while (fscanf(fp, "%s %u %u %u \n", sig, &obj, &called, &drop) == 4) {
			string_to_sig(sig, &sigval);
			fh = (filter_history_t *) 
				  g_hash_table_lookup(histories, (gconstpointer) &sigval);
			
			if (fh == NULL) {
				fh = g_malloc0(sizeof(filter_history_t));
				string_to_sig(sig, &fh->filter_sig);
				g_hash_table_insert(histories, &fh->filter_sig, fh);				
			}
			fh->executions++;
			fh->search_objects += obj;
			fh->filter_objects += called;
			fh->drop_objects += drop;
		}
		fclose(fp);
		if (remove == TRUE) 
			unlink(fname);
	}
	
	closedir(dir);
	free(path);
	return;
}
