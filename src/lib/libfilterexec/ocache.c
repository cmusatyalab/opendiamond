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

#ifdef DEBUG
#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

/*
 * dctl variables 
 */
static unsigned int if_cache_table = 1;
static unsigned int if_cache_oattr = 1;

#define OCACHE_DB_NAME "/ocache.db"
static sqlite3 *ocache_DB;

static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_FILTER_ARG_NAME 256

/* assume we can read approximately 1MB/s of attribute data from the cache,
 * this should at somepoint (when we actually read attributes) become a
 * running average of the measured bandwidth. */
#define ESTIMATED_ATTR_READ_BW (1024LL*1024LL)

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

#if 0
	/* the filter should already be uniquely defined based on the
	 * filter data and attributes/parameters */
	iov[n].iov_base = fn_name;
	iov[n].iov_len = strlen(fn_name);
	n++;
#endif

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
"PRAGMA temp_store = MEMORY;"
"PRAGMA synchronous = OFF;"
"BEGIN TRANSACTION;"
"CREATE TABLE IF NOT EXISTS cache ("
"    cache_entry INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
"    object_sig  BLOB NOT NULL,"
"    filter_sig  BLOB,"
"    confidence  INTEGER NOT NULL,"
"    create_time INTEGER," /* DEFAULT strftime('%s', 'now', 'utc')"*/
"    elapsed_ms  INTEGER"
"); "
"CREATE INDEX IF NOT EXISTS object_filter_idx ON cache (object_sig,filter_sig);"
""
"CREATE TABLE IF NOT EXISTS attrs ("
"    sig	BLOB PRIMARY KEY NOT NULL ON CONFLICT IGNORE,"
"    value	BLOB"
");"
""
"CREATE TABLE IF NOT EXISTS input_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    name	 TEXT NOT NULL,"
"    sig	 BLOB NOT NULL,"
"    PRIMARY KEY (cache_entry, name) ON CONFLICT REPLACE"
");"
""
"CREATE TABLE IF NOT EXISTS output_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    name	 TEXT NOT NULL,"
"    sig	 BLOB NOT NULL,"
"    PRIMARY KEY (cache_entry, name) ON CONFLICT REPLACE"
");"
//"CREATE INDEX IF NOT EXISTS output_attr_idx ON output_attrs (cache_entry);"
""
"CREATE TEMP TABLE current_attrs ("
"    name   TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE,"
"    sig    BLOB NOT NULL"
");"
//"CREATE INDEX IF NOT EXISTS current_attr_idx ON current_attrs (name, sig);"
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
	sqlite3_stmt *res;
	int found = 0;

	if (if_cache_table == 0 || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache lookup\n");

	sql_query(&res, ocache_DB,
		  "SELECT cache_entry, confidence FROM cache"
		  " WHERE object_sig = ?1 AND filter_sig = ?2 AND"
		  " cache_entry NOT IN (SELECT cache_entry"
		  "   FROM cache JOIN input_attrs USING(cache_entry)"
		  "   LEFT OUTER JOIN current_attrs USING(name, sig)"
		  "   WHERE object_sig = ?1 AND filter_sig = ?2 AND"
		  "   current_attrs.name ISNULL)"
		  " LIMIT 1;",
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
		  "INSERT INTO current_attrs (name, sig)"
		  "  SELECT name, sig FROM output_attrs"
		  "  WHERE cache_entry = ?1;",
		  "D", cache_entry);

	pthread_mutex_unlock(&shared_mutex);
}

/* load cached output attributes for a object (called from odisk_pr_load) */
int
cache_read_oattrs(obj_attr_t *attr, int64_t cache_entry)
{
	sqlite3_stmt *res;
	int ret, err = ENOENT; /* return a non-zero error if we had no
				  attributes cached for this cache_entry */
	char *name;
	void *value;
	int length;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache read oattr\n");

	ret = sql_query(&res, ocache_DB,
			"SELECT name, value "
			" FROM output_attrs JOIN attrs USING(sig)"
			" WHERE cache_entry = ?1;",
			"D", cache_entry);

	while (ret == SQLITE_ROW) {
		sql_query_row(res, "sb", &name, &value, &length);
		err = obj_write_attr(attr, name, length, value);
		ret = sql_query_next(res);
		if (err) break;
	}

	sql_query_free(res);
	pthread_mutex_unlock(&shared_mutex);
	return err;
}

/* Build state to keep track of initial object attributes. */
void
ocache_add_initial_attrs(lf_obj_handle_t ohandle)
{
	obj_data_t *obj = (obj_data_t *)ohandle;
	unsigned char *buf;
	size_t len;
	void *cookie;
	attr_record_t *arec;
	int ret, rc;
	sqlite_int64 rowid;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache add initial attrs\n");

	rc = sql_begin(ocache_DB);
	if (rc != SQLITE_OK) goto out;

	rc = sql_query(NULL, ocache_DB,
		"INSERT INTO cache"
		" (object_sig, confidence, create_time, elapsed_ms)"
		" VALUES (?1, 100, strftime('%s', 'now', 'utc'), 0);",
		"B", &obj->id_sig, sizeof(sig_val_t));
	if (rc != SQLITE_OK) goto out_fail;

	rowid = sqlite3_last_insert_rowid(ocache_DB);

	ret = obj_get_attr_first(&obj->attr_info, &buf, &len, &cookie, 0);
	while (ret != ENOENT && rc == SQLITE_OK) {
		if (buf == NULL) {
			printf("can not get attr\n");
			break;
		}
		arec = (attr_record_t *) buf;

		rc = sql_query(NULL, ocache_DB,
			       "INSERT INTO output_attrs (cache_entry,name,sig)"
			       "  VALUES (?1, ?2, ?3);", "DSB",
			       rowid, arec->data,
			       &arec->attr_sig, sizeof(sig_val_t));

		ret = obj_get_attr_next(&obj->attr_info, &buf, &len, &cookie,0);
	}

out_fail:
	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
	else	sql_commit(ocache_DB);
out:
	pthread_mutex_unlock(&shared_mutex);
}

int
cache_reset_current_attrs(query_info_t *qid, sig_val_t *idsig)
{
	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache reset current attrs\n");

	sql_begin(ocache_DB);

	sql_query(NULL, ocache_DB, "DELETE FROM current_attrs;", NULL);
	sql_query(NULL, ocache_DB,
		  "INSERT INTO current_attrs (name, sig)"
		  " SELECT name, sig FROM cache JOIN output_attrs"
		  " USING(cache_entry)"
		  " WHERE object_sig = ?1 and filter_sig ISNULL;",
		  "B", idsig, sizeof(sig_val_t));

	sql_commit(ocache_DB);

	pthread_mutex_unlock(&shared_mutex);
	return 1;
}

int
ocache_add_start(lf_obj_handle_t ohandle, sig_val_t *fsig)
{
	int rc;

	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);

	rc = sql_begin(ocache_DB);
	if (rc != SQLITE_OK) goto out;

	rc = sql_query(NULL, ocache_DB,
		       "CREATE TEMP TABLE temp_iattrs ("
		       "    name TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE,"
		       "    sig  BLOB NOT NULL"
		       ");", NULL);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB,
		       "CREATE TEMP TABLE temp_oattrs ("
		       "    name TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE,"
		       "    sig  BLOB NOT NULL,"
		       "    value BLOB"
		       ");", NULL);
out_fail:
	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
	else	sql_commit(ocache_DB);
out:
	pthread_mutex_unlock(&shared_mutex);

	/* we currently don't support concurrent filter executions */
	assert(rc == SQLITE_OK);

	return rc;
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
		  "INSERT INTO temp_iattrs (name, sig) VALUES (?1, ?2);",
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

	if (if_cache_oattr)
		value = data;

	sql_query(NULL, ocache_DB,
		  "INSERT INTO temp_oattrs (name, sig, value)"
		  "  VALUES (?1, ?2, ?3);",
		  "SBB", name, &sig, sizeof(sig_val_t), value, len);

	pthread_mutex_unlock(&shared_mutex);
}

int
ocache_add_end(lf_obj_handle_t ohandle, sig_val_t *fsig, int conf,
	       query_info_t *qinfo, filter_exec_mode_t exec_mode,
	       struct timespec *elapsed)
{
	sqlite3_stmt *res;
	obj_data_t *obj = (obj_data_t *) ohandle;
	int elapsed_ms;
	sqlite_int64 rowid, oattr_size;
	int rc;

	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache add filter results\n");

	rc = sql_begin(ocache_DB);
	if (rc != SQLITE_OK) goto out;

	elapsed_ms = (elapsed->tv_sec * 1000) + (elapsed->tv_nsec / 1000000);

	rc = sql_query(NULL, ocache_DB,
		"INSERT INTO cache"
		" (object_sig, filter_sig, confidence, create_time, elapsed_ms)"
		" VALUES (?1, ?2, ?3, strftime('%s', 'now', 'utc'), ?4);",
		"BBdd",&obj->id_sig, sizeof(sig_val_t), fsig, sizeof(sig_val_t),
		conf, elapsed_ms);
	if (rc != SQLITE_OK) goto out_fail;

	rowid = sqlite3_last_insert_rowid(ocache_DB);

	rc = sql_query(NULL, ocache_DB,
		       "INSERT INTO input_attrs (cache_entry, name, sig)"
		       "  SELECT ?1, name, sig FROM temp_iattrs;",
		       "D", rowid);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sql_query(NULL, ocache_DB,
		       "INSERT INTO output_attrs (cache_entry, name, sig)"
		       "  SELECT ?1, name, sig FROM temp_oattrs;",
		       "D", rowid);
	if (rc != SQLITE_OK) goto out_fail;

	if (!if_cache_oattr)
		goto out;

	sql_query(&res, ocache_DB,
		  "SELECT sum(length(value)) FROM temp_oattrs;", NULL);
	if (!res) goto out;

	sql_query_row(res, "D", &oattr_size);
	sql_query_free(res);

	/* if it took a long time to generate a small amount of data, it
	 * should be useful to cache the results so that we can read the
	 * attributes from the cache instead of reexecuting the filter. */
	if ((oattr_size*1000LL) < (ESTIMATED_ATTR_READ_BW*(int64_t)elapsed_ms))
	{
	    sql_query(NULL, ocache_DB,
		      "INSERT INTO attrs (sig, value)"
		      "  SELECT sig, value FROM temp_oattrs;", NULL);
	}
out_fail:
	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
out:
	sql_query(NULL, ocache_DB, "DROP TABLE temp_iattrs;", NULL);
	sql_query(NULL, ocache_DB, "DROP TABLE temp_oattrs;", NULL);

	if (rc == SQLITE_OK)
		sql_commit(ocache_DB);

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

