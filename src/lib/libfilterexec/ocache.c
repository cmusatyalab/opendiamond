/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007 Carnegie Mellon University
 *  All rights reserved.
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include <assert.h>
#include <sys/uio.h>
#include <limits.h>

#include "sql.h"

#include "diamond_consts.h"
#include "diamond_types.h"
#include "lib_tools.h"
#include "lib_log.h"
#include "lib_dctl.h"
#include "dctl_common.h"
#include "ocache_priv.h"
#include "obj_attr.h"
#include "obj_attr.h"
#include "lib_filter.h"
#include "lib_filter_sys.h"
#include "lib_odisk.h"
#include "lib_filterexec.h"
#include "lib_ocache.h"
#include "lib_dconfig.h"
#include "filter_priv.h"

#ifndef NDEBUG
#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

/*
 * dctl variables 
 */
static unsigned int if_cache_table = 1;
static unsigned int if_cache_oattr = 0;

#define OCACHE_DB_NAME "/ocache.db"
static sqlite3 *ocache_DB;

static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_FILTER_ARG_NAME 256

/*
 * This could be moved to a support library XXX
 */
int
digest_cal(filter_data_t * fdata, char *fn_name, int numarg, char **filt_args,
	   int blob_len, void *blob, sig_val_t * signature)
{
	struct ciovec *iov;
	int i, len, n = 0;

	len =	fdata->num_libs +	/* library_signatures */
		1 +			/* filter name */
		numarg +		/* filter arguments */
		1;			/* optional binary blob */

	iov = (struct ciovec *)malloc(len * sizeof(struct ciovec));
	assert(iov != NULL);

	/* include the library signatures */
	for (i = 0; i < fdata->num_libs; i++) {
		iov[n].iov_base = &fdata->lib_info[i].lib_sig;
		iov[n].iov_len = sizeof(sig_val_t);
		n++;
	}

	iov[n].iov_base = fn_name;
	iov[n].iov_len = strlen(fn_name);
	n++;

	/* include the args */
	for (i = 0; i < numarg; i++) {
		if (filt_args[i] == NULL)
			continue;

		len = strlen(filt_args[i]);
		if (len >= MAX_FILTER_ARG_NAME)
			return (EINVAL);

		iov[n].iov_base = filt_args[i];
		iov[n].iov_len = len;
		n++;
	}

	/* include binary blob */
	if (blob_len > 0) {
		iov[n].iov_base = blob;
		iov[n].iov_len = blob_len;
		n++;
	}

	sig_cal_vec(iov, n, signature);
	free(iov);
	return 0;
}

static void
cache_setup(const char *dir)
{
	char *db_file, *errmsg = NULL;
	int rc;

	if (ocache_DB != NULL) return;

	db_file = malloc(strlen(dir) + strlen(OCACHE_DB_NAME) + 1);
	strcpy(db_file, dir);
	strcat(db_file, OCACHE_DB_NAME);

	debug("Opening ocache database\n");
	rc = sqlite3_open(db_file, &ocache_DB);

	free(db_file);

	if (rc != SQLITE_OK) {
		ocache_DB = NULL;
		return;
	}

	debug("Initializing... ");
	rc = sqlite3_exec(ocache_DB,
"BEGIN TRANSACTION;"
"CREATE TABLE IF NOT EXISTS cache ("
"    cache_entry INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
"    filter_sig	BLOB NOT NULL,"
"    object_sig	BLOB NOT NULL,"
"    confidence	INTEGER NOT NULL,"
"    create_time INTEGER" /* DEFAULT strftime('%s', 'now', 'utc')"*/
/*"    create_time TEXT DEFAULT CURRENT_TIMESTAMP"*/
"); "
"CREATE INDEX IF NOT EXISTS filter_object_idx ON cache (filter_sig,object_sig);"
""
"CREATE TABLE IF NOT EXISTS attrs ("
"    attr_id	INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
"    name	TEXT NOT NULL,"
"    sig	BLOB NOT NULL,"
"    value	BLOB,"
"    UNIQUE (name, sig) ON CONFLICT IGNORE"
");"
""
"CREATE TABLE IF NOT EXISTS input_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    attr_id     INTEGER NOT NULL,"
"    PRIMARY KEY (cache_entry, attr_id) ON CONFLICT IGNORE"
");"
""
"CREATE TABLE IF NOT EXISTS output_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    attr_id     INTEGER NOT NULL,"
"    PRIMARY KEY (cache_entry, attr_id) ON CONFLICT IGNORE"
");"
""
"CREATE TEMP TABLE current_attrs ("
"    attr_id    INTEGER NOT NULL,"
"    PRIMARY KEY (attr_id) ON CONFLICT IGNORE"
");"
""
"CREATE TEMP TABLE temp_iattrs ("
"    attr_id    INTEGER NOT NULL,"
"    PRIMARY KEY (attr_id) ON CONFLICT IGNORE"
");"
"CREATE TEMP TABLE temp_oattrs ("
"    name	TEXT NOT NULL,"
"    sig	BLOB NOT NULL,"
"    value      BLOB,"
"    UNIQUE (name) ON CONFLICT REPLACE"
");"
"COMMIT TRANSACTION;" , NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
	    fprintf(stderr, "ocache.db initialization failed: %s\n", errmsg);
	    sqlite3_free(errmsg);
	    ocache_DB = NULL;
    }
    debug("done\n");

    sql_set_busy_handler(ocache_DB);
}

int
cache_lookup(sig_val_t *idsig, sig_val_t *fsig, query_info_t *qid,
	     int *confidence, int64_t *cache_entry)
{
	static sqlite3_stmt *res;
	int found = 0;

	if (if_cache_table == 0 || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache lookup\n");

	sql_query(&res, ocache_DB,
		  "SELECT cache_entry, confidence FROM cache"
		  "  WHERE object_sig = ?1 AND filter_sig = ?2 AND"
		  "  cache_entry NOT IN (SELECT cache.cache_entry"
		  "    FROM cache, input_attrs USING(cache_entry)"
		  "    WHERE object_sig = ?1 AND filter_sig = ?2 AND"
		  "    input_attrs.attr_id NOT IN current_attrs)"
		  "  LIMIT 1;",
		  "BB", idsig, sizeof(sig_val_t), fsig, sizeof(sig_val_t));

	if (res) {
		sql_query_row(res, "Dd", cache_entry, confidence);
		sql_query_free(res);
		found = 1;
	}

	pthread_mutex_unlock(&shared_mutex);
	return found;
}

void
cache_combine_attr_set(query_info_t *qid, int64_t cache_entry)
{
	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache combine attr set\n");

	sql_query(NULL, ocache_DB,
		  "INSERT INTO current_attrs (attr_id)"
		  "  SELECT attr_id FROM output_attrs"
		  "  WHERE cache_entry = ?1;",
		  "D", cache_entry);

	pthread_mutex_unlock(&shared_mutex);
}

/* Build state to keep track of initial object attributes. */
void
cache_set_init_attrs(sig_val_t *idsig, obj_attr_t *init_attr)
{
#if 0
	unsigned char *buf;
	size_t len;
	void *cookie;
	attr_record_t *arec;
	int64_t rowid;
	int ret, rc;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache set init attrs\n");

	rc = sql_begin(ocache_DB);

	ret = obj_get_attr_first(init_attr, &buf, &len, &cookie, 0);
	while (ret != ENOENT && rc == SQLITE_OK) {
		if (buf == NULL) {
			printf("can not get attr\n");
			break;
		}
		arec = (attr_record_t *) buf;

		rc = sql_query(NULL, ocache_DB,
			       "INSERT INTO attrs (name, sig) VALUES (?1, ?2);"
			       "SB", arec->data,
			       &arec->attr_sig, sizeof(sig_val_t));
		if (rc != SQLITE_OK)
			break;

		rowid = sqlite3_last_insert_rowid(ocache_DB);

		rc = sql_query(NULL, ocache_DB,
			       "INSERT INTO initial_attrs (object_sig, attr_id)"
			       "  VALUES (?1, ?2);",
			       "BD", idsig, sizeof(sig_val_t), rowid);

		ret = obj_get_attr_next(init_attr, &buf, &len, &cookie, 0);
	}

	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
	else	sql_commit(ocache_DB);

	pthread_mutex_unlock(&shared_mutex);
#endif
}

int
cache_get_init_attrs(query_info_t *qid, sig_val_t *idsig)
{
	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache get init attrs\n");

	sql_query(NULL, ocache_DB, "DELETE FROM current_attrs;", NULL);

	pthread_mutex_unlock(&shared_mutex);
	return 1;
}

int
ocache_add_start(lf_obj_handle_t ohandle, sig_val_t *fsig)
{
	/* check if we don't have temp_iattr/temp_oattr entries for this
	 * query or object? */
	return 0;
}

static void
ocache_add_iattr(lf_obj_handle_t ohandle,
		 const char *name, off_t len, const unsigned char *data)
{
	obj_data_t *obj = (obj_data_t *) ohandle;
	sig_val_t sig;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache insert temporary iattr '%s'\n", name);

	odisk_get_attr_sig(obj, name, &sig);

	sql_query(NULL, ocache_DB,
		  "INSERT INTO temp_iattrs (attr_id)"
		  "  SELECT attr_id FROM current_attrs, attrs USING (attr_id)"
		  "  WHERE attrs.name = ?1 AND attrs.sig = ?2;",
		  "SB", name, &sig, sizeof(sig_val_t));

	pthread_mutex_unlock(&shared_mutex);
}

static void
ocache_add_oattr(lf_obj_handle_t ohandle, const char *name,
		 off_t len, const unsigned char *data)
{
	obj_data_t *obj = (obj_data_t *) ohandle;
	sig_val_t sig;
	const void *value = NULL;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	/* call function to update stats */
	ceval_wattr_stats(len);

	pthread_mutex_lock(&shared_mutex);
	debug("Cache insert temporary oattr '%s'\n", name);

	odisk_get_attr_sig(obj, name, &sig);

	if (if_cache_oattr && data != NULL /* && len < magic_value */)
		value = data;

	sql_query(NULL, ocache_DB,
		  "INSERT INTO temp_oattrs (name, sig, value)"
		  "  VALUES (?1, ?2, ?3);",
		  "SBB", name, &sig, sizeof(sig_val_t), value, len);

	pthread_mutex_unlock(&shared_mutex);
}

int
ocache_add_end(lf_obj_handle_t ohandle, sig_val_t *fsig, int conf,
	       query_info_t *qinfo, filter_exec_mode_t exec_mode)
{
	obj_data_t *obj = (obj_data_t *) ohandle;
	sqlite_int64 rowid;
	int rc;

	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache add filter results\n");

	rc = sql_begin(ocache_DB);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB,
		       "INSERT INTO cache (filter_sig, object_sig, confidence,"
		       "		      create_time)"
		       "  VALUES (?1, ?2, ?3, strftime('%s', 'now', 'utc'));",
		       "BBd", fsig, sizeof(sig_val_t),
		       &obj->id_sig, sizeof(sig_val_t), conf);
	if (rc != SQLITE_OK) goto out_fail;

	rowid = sqlite3_last_insert_rowid(ocache_DB);

	rc = sql_query(NULL, ocache_DB,
		       "INSERT INTO attrs (name, sig, value)"
		       "  SELECT name, sig, value FROM temp_oattrs;", NULL);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB,
		       "INSERT INTO output_attrs (cache_entry, attr_id)"
		       "  SELECT ?1, attr_id FROM temp_oattrs, attrs"
		       "  USING(name, sig);",
		       "D", rowid);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB, "DELETE FROM temp_oattrs;", NULL);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB,
		       "INSERT INTO input_attrs (cache_entry, attr_id)"
		       "  SELECT ?1, attr_id FROM temp_iattrs;",
		       "D", rowid);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB, "DELETE FROM temp_iattrs;", NULL);

out_fail:
	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
	else	sql_commit(ocache_DB);

	pthread_mutex_unlock(&shared_mutex);
	return 0;
}

int
ocache_init(char *dirp)
{
	int  err;
	char *dir_path;

	if (dirp == NULL) {
		dir_path = dconf_get_cachedir();
	} else {
		dir_path = strdup(dirp);
	}
	if (strlen(dir_path) > (MAX_DIR_PATH - 1)) {
		return (EINVAL);
	}
	err = mkdir(dir_path, 0777);
	if (err && errno != EEXIST) {
		printf("fail to creat cache dir (%s), err %d\n", dir_path,
		       errno);
		free(dir_path);
		return (EPERM);
	}

	/*
	 * dctl control 
	 */
	dctl_register_leaf(DEV_CACHE_PATH, "cache_table", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &if_cache_table);
	dctl_register_leaf(DEV_CACHE_PATH, "cache_oattr", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &if_cache_oattr);

	/*
	 * set callback functions so we get notifice on read/and writes
	 * to object attributes.
	 */
	lf_set_read_cb(ocache_add_iattr);
	lf_set_write_cb(ocache_add_oattr);

	/* open and initialize ocache database */
	cache_setup(dir_path);
	free(dir_path);
	return (0);
}

int
ocache_start(void)
{
	return 0;
}

/*
 * called by search_close_conn in adiskd
 */
int
ocache_stop(char *dirp)
{
	return 0;
}

/*
 * called by ceval_stop, ceval_stop is called when Stop 
 */
int
ocache_stop_search(sig_val_t * fsig)
{
	return 0;
}

