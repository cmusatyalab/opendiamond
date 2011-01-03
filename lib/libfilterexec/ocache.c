/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (c) 2002-2005 Intel Corporation
 *  Copyright (c) 2006 Larry Huston <larry@thehustons.net>
 *  Copyright (c) 2007-2009 Carnegie Mellon University
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
#include "ocache_priv.h"
#include "obj_attr.h"
#include "obj_attr.h"
#include "lib_filter.h"
#include "lib_filter_sys.h"
#include "lib_odisk.h"
#include "lib_filterexec.h"
#include "lib_ocache.h"
#include "dconfig_priv.h"
#include "odisk_priv.h"
#include "filter_priv.h"
#include "sig_calc_priv.h"


#ifdef DEBUG
#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

static unsigned int if_cache_table = 1;
static unsigned int if_cache_oattr = 1;

#define OCACHE_DB_NAME "/ocache.db"
#define OATTR_DB_NAME "/oattr.db"
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
digest_cal(filter_data_t * fdata, char *filter_name, char *function_name,
	   int numarg, char **filt_args, int blob_len, void *blob,
	   sig_val_t * signature)
{
	struct ciovec *iov;
	int i, len, n = 0;

	len =	fdata->num_libs +	/* library_signatures */
		2 +			/* filter/function name */
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

	/* The filter should already be uniquely defined based on the
	 * filter data and attributes/parameters.
	 * However some filters use their name as part of the generated output
	 * attributes and so differently named executions of the same filter
	 * should not be considered identical. */
	iov[n].iov_base = filter_name;
	iov[n].iov_len = strlen(filter_name);
	n++;

	iov[n].iov_base = function_name;
	iov[n].iov_len = strlen(function_name);
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
	sqlite3_stmt *res;
	int version;
	int rc;

	if (ocache_DB != NULL) return;

	fprintf(stderr, "Opening ocache database\n");
	db_file = malloc(strlen(dir) + strlen(OCACHE_DB_NAME) + 1);
	strcpy(db_file, dir); strcat(db_file, OCACHE_DB_NAME);
	rc = sqlite3_open(db_file, &ocache_DB);
	free(db_file);

	if (rc != SQLITE_OK) {
		ocache_DB = NULL;
		return;
	}

	/* attach oattr database file */
	fprintf(stderr, "Opening oattr database\n");
	db_file = malloc(strlen(dir) + strlen(OATTR_DB_NAME) + 1);
	strcpy(db_file, dir); strcat(db_file, OATTR_DB_NAME);
	rc = sql_attach(ocache_DB, "oattr", db_file);
	free(db_file);

	if (rc != SQLITE_OK) {
		sqlite3_close(ocache_DB);
		ocache_DB = NULL;
		return;
	}

	sql_set_busy_handler(ocache_DB);

	sql_query(NULL, ocache_DB, "PRAGMA temp_store = MEMORY;", NULL);
	sql_query(NULL, ocache_DB, "PRAGMA synchronous = OFF;", NULL);
	sql_query(&res, ocache_DB, "PRAGMA user_version;", NULL);
	assert(res); /* PRAGMA user_version should always succeed */

	sql_query_row(res, "d", &version);
	sql_query_free(res);

	sql_begin(ocache_DB);

	switch (version) {
	case 0: /* Initializing a new db */
	    fprintf(stderr, "Initializing new database... ");
	    break;

	case 1: /* apply any necessary upgrades to the schema */
	    /* move tables/indices that need to be rebuilt aside */
	    fprintf(stderr, "Upgrading database... ");
	    rc = sqlite3_exec(ocache_DB,
			      "DROP INDEX object_filter_idx;"
			      "ALTER TABLE cache RENAME TO old_cache;",
			      NULL, NULL, &errmsg);
	    if (rc != SQLITE_OK) goto err_out;
	    break;

	case 2: /* current version */
	    fprintf(stderr, "Database up-to-date... ");
	    break;

	default: /* future versions */
	    errmsg = strdup("Unrecognized ocache.db version");
	    rc = SQLITE_ERROR;
	    goto err_out;
	}

	rc = sqlite3_exec(ocache_DB,
"CREATE TABLE IF NOT EXISTS cache ("
"    cache_entry INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
"    object_sig  BLOB NOT NULL,"
"    filter_sig  BLOB,"
"    score	 INTEGER NOT NULL,"
"    create_time INTEGER," /* DEFAULT strftime('%s', 'now', 'utc')"*/
"    elapsed_ms  INTEGER"
"); "
""
"CREATE TABLE IF NOT EXISTS input_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    name	 TEXT NOT NULL,"
"    sig	 BLOB NOT NULL,"
"    PRIMARY KEY (cache_entry, name)"
");"
""
"CREATE TABLE IF NOT EXISTS output_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    name	 TEXT NOT NULL,"
"    sig	 BLOB NOT NULL,"
"    PRIMARY KEY (cache_entry, name)"
");"
""
"CREATE TABLE IF NOT EXISTS oattr.attrs ("
"    name	TEXT NOT NULL,"
"    sig	BLOB NOT NULL,"
"    value	BLOB NOT NULL,"
"    PRIMARY KEY (sig, name)"
");", NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) goto err_out;

	/* now that any missing tables have been created, we can copy data
	 * from the old tables into the new schema */
	switch (version) {
	case 1:
	    rc = sqlite3_exec(ocache_DB,
		"INSERT INTO cache SELECT cache_entry, object_sig, filter_sig,"
		"    confidence, create_time, elapsed_ms FROM old_cache;"
		"DROP TABLE old_cache;"
		"INSERT OR IGNORE INTO oattr.attrs SELECT name, sig, value"
		"    FROM output_attrs JOIN attrs USING(sig);"
		"DROP TABLE attrs;", NULL, NULL, &errmsg);
	    if (rc != SQLITE_OK) goto err_out;

	    fprintf(stderr, "upgraded schema... ");
	default:
	    break;
	}

	/* and finally we can rebuild the indices and set the schema version */
	rc = sqlite3_exec(ocache_DB,
	    "CREATE INDEX IF NOT EXISTS object_filter_idx"
	    "    ON cache (object_sig, filter_sig);"
	    "PRAGMA user_version = 2;", NULL, NULL, &errmsg);
	if (rc != SQLITE_OK) goto err_out;
	sql_commit(ocache_DB);

	fprintf(stderr, "temporary tables... ");
	sql_begin(ocache_DB);
	rc = sqlite3_exec(ocache_DB,
"CREATE TEMP TABLE current_attrs ("
"    name	TEXT PRIMARY KEY NOT NULL,"
"    sig	BLOB NOT NULL"
");"
"CREATE TEMP TABLE temp_iattrs ("
"    name	TEXT PRIMARY KEY NOT NULL,"
"    sig	BLOB NOT NULL"
");"
"CREATE TEMP TABLE temp_oattrs ("
"    name	TEXT PRIMARY KEY NOT NULL,"
"    sig	BLOB NOT NULL,"
"    length	INTEGER"
");", NULL, NULL, &errmsg);

err_out:
	if (rc != SQLITE_OK) {
		fprintf(stderr, "disabling cache\n");
		fprintf(stderr, "Cache initialization failed: %s\n", errmsg);
		/* if (errmsg) free(errmsg); *** got a bad pointer to free,
		 * not sure what sqlite really returns here. */

		sql_rollback(ocache_DB);
		sqlite3_close(ocache_DB);
		ocache_DB = NULL;
	} else {
		sql_commit(ocache_DB);
		fprintf(stderr, "done\n");
	}
}

int
cache_lookup(sig_val_t *idsig, sig_val_t *fsig, query_info_t *qid,
	     int *score, int64_t *cache_entry)
{
	sqlite3_stmt *res = NULL;
	int found = 0;

	if (ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache lookup\n");

	sql_query(&res, ocache_DB,
		  "SELECT cache_entry, score FROM cache"
		  " WHERE object_sig = ?1 AND filter_sig = ?2 AND"
		  " cache_entry NOT IN (SELECT cache_entry"
		  "   FROM cache JOIN input_attrs USING(cache_entry)"
		  "   LEFT OUTER JOIN current_attrs USING(name, sig)"
		  "   WHERE object_sig = ?1 AND filter_sig = ?2 AND"
		  "   current_attrs.name ISNULL)"
		  " LIMIT 1;",
		  "BB", idsig, sizeof(sig_val_t), fsig, sizeof(sig_val_t));

	if (res) {
		sql_query_row(res, "Dd", cache_entry, score);
		sql_query_free(res);
		found = 1;
	}

	pthread_mutex_unlock(&shared_mutex);
	return found;
}

void
cache_combine_attr_set(query_info_t *qid, int64_t cache_entry)
{
	if (ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache combine attr set\n");

	sql_query(NULL, ocache_DB,
		  "INSERT OR REPLACE INTO current_attrs (name, sig)"
		  "  SELECT name, sig FROM output_attrs"
		  "  WHERE cache_entry = ?1;",
		  "D", cache_entry);

	pthread_mutex_unlock(&shared_mutex);
}

/* load cached output attributes for a object (called from odisk_pr_load) */
int
cache_read_oattrs(obj_attr_t *attr, int64_t cache_entry)
{
	sqlite3_stmt *res = NULL;
	int ret, err = ENOENT; /* return a non-zero error if we had no
				  attributes cached for this cache_entry */
	char *name;
	void *value;
	int length;

	if (ocache_DB == NULL)
		return ENOENT;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache read oattr\n");

	ret = sql_query(&res, ocache_DB,
			"SELECT output_attrs.name, value "
			" FROM output_attrs LEFT JOIN oattr.attrs USING(sig)"
			" WHERE cache_entry = ?1;",
			"D", cache_entry);

	while (ret == SQLITE_ROW) {
		sql_query_row(res, "sb", &name, &value, &length);
		if (!value) { err = ENOENT; break; }
		err = obj_write_attr(attr, name, length, value);
		if (err) break;
		ret = sql_query_next(res);
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
	char *attr_name;
	sig_val_t *attr_sig;
	struct acookie *cookie = NULL;
	int ret, rc;
	sqlite_int64 rowid;
	sqlite3_stmt *res = NULL;
	sqlite_int64 have_initial_attrs = 0;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	/* check if there are any initial attributes to add */
	ret = obj_first_attr(&obj->attr_info, &attr_name, NULL, NULL, &attr_sig,
			     &cookie);
	if (ret == ENOENT) return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache add initial attrs\n");

	rc = sql_begin(ocache_DB);
	if (rc != SQLITE_OK) goto out;

	rc = sql_query(&res, ocache_DB,
		"SELECT COUNT(*) FROM cache WHERE"
		" object_sig = ?1 AND filter_sig ISNULL",
		"B", &obj->id_sig, sizeof(sig_val_t));
	if (res) {
		sql_query_row(res, "D", &have_initial_attrs);
		sql_query_free(res);
	}
	if (have_initial_attrs)
		goto out_fail;

	rc = sql_query(NULL, ocache_DB,
		"INSERT INTO cache"
		" (object_sig, score, create_time, elapsed_ms)"
		" VALUES (?1, 1, strftime('%s', 'now', 'utc'), 0);",
		"B", &obj->id_sig, sizeof(sig_val_t));
	if (rc != SQLITE_OK) goto out_fail;

	rowid = sqlite3_last_insert_rowid(ocache_DB);

	while (ret != ENOENT) {
		if (rc == SQLITE_OK) {
			rc = sql_query(NULL, ocache_DB,
		    "INSERT OR REPLACE INTO output_attrs (cache_entry,name,sig)"
			       "  VALUES (?1, ?2, ?3);", "DSB",
			       rowid, attr_name, attr_sig, sizeof(sig_val_t));
		}
		ret = obj_next_attr(&obj->attr_info, &attr_name, NULL, NULL,
				    &attr_sig, &cookie);
	}

out_fail:
	if (cookie)
		free(cookie);

	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
	else	sql_commit(ocache_DB);
out:
	pthread_mutex_unlock(&shared_mutex);
}

int
cache_reset_current_attrs(query_info_t *qid, sig_val_t *idsig)
{
	if (ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache reset current attrs\n");

	sql_begin(ocache_DB);

	sql_query(NULL, ocache_DB, "DELETE FROM current_attrs;", NULL);
	sql_query(NULL, ocache_DB,
		  "INSERT OR REPLACE INTO current_attrs (name, sig)"
		  " SELECT name, sig FROM cache JOIN output_attrs"
		  " USING(cache_entry)"
		  " WHERE object_sig = ?1 AND filter_sig ISNULL;",
		  "B", idsig, sizeof(sig_val_t));

	sql_commit(ocache_DB);

	pthread_mutex_unlock(&shared_mutex);
	return 1;
}

int
ocache_add_start(lf_obj_handle_t ohandle, sig_val_t *fsig)
{
	sqlite3_stmt *res = NULL;
	sqlite_int64 nattr = 0;

	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Start of filter execution\n");

	sql_query(&res, ocache_DB, "SELECT COUNT(*) FROM temp_oattrs;", NULL);
	if (res) {
		sql_query_row(res, "D", &nattr);
		sql_query_free(res);
	}

	pthread_mutex_unlock(&shared_mutex);

	/* we currently don't support concurrent filter executions */
	assert(nattr == 0);

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
	    "INSERT OR REPLACE INTO temp_iattrs (name, sig) VALUES (?1, ?2);",
		  "SB", name, &sig, sizeof(sig_val_t));

	pthread_mutex_unlock(&shared_mutex);
}

static void
ocache_add_oattr(lf_obj_handle_t ohandle, const char *name,
		 off_t len, const unsigned char *data)
{
	obj_data_t *obj = (obj_data_t *) ohandle;
	sig_val_t sig;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	/* call function to update stats */
	ceval_wattr_stats(len);

	pthread_mutex_lock(&shared_mutex);
	debug("Cache insert temporary oattr '%s'\n", name);

	odisk_get_attr_sig(obj, name, &sig);

	sql_query(NULL, ocache_DB,
		  "INSERT OR REPLACE INTO temp_oattrs (name, sig, length)"
		  " VALUES (?1, ?2, ?3);", "SBd",
		  name, &sig, sizeof(sig_val_t), len);

	pthread_mutex_unlock(&shared_mutex);
}

int
ocache_add_end(lf_obj_handle_t ohandle, sig_val_t *fsig, int score,
	       query_info_t *qinfo, struct timespec *elapsed)
{
	sqlite3_stmt *res;
	obj_data_t *obj = (obj_data_t *) ohandle;
	int elapsed_ms;
	sqlite_int64 rowid, oattr_size;
	int rc;

	if (ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache add filter results\n");

	/* if caching is disabled, we still want to make sure that the
	 * temporary tables are cleared in case caching was disabled during
	 * the execution of this filter */
	rc = SQLITE_ERROR;
	if (if_cache_table == 0)
		goto out;

	rc = sql_begin(ocache_DB);
	if (rc != SQLITE_OK)
		goto out;

	elapsed_ms = (elapsed->tv_sec * 1000) + (elapsed->tv_nsec / 1000000);

	rc = sql_query(NULL, ocache_DB,
		"INSERT INTO cache"
		" (object_sig, filter_sig, score, create_time, elapsed_ms)"
		" VALUES (?1, ?2, ?3, strftime('%s', 'now', 'utc'), ?4);",
		"BBdd",&obj->id_sig, sizeof(sig_val_t), fsig, sizeof(sig_val_t),
		score, elapsed_ms);
	if (rc != SQLITE_OK) goto out_fail;

	rowid = sqlite3_last_insert_rowid(ocache_DB);

	debug("Cache add input attributes\n");
	rc = sql_query(NULL, ocache_DB,
		"INSERT OR REPLACE INTO input_attrs (cache_entry, name, sig)"
		       "  SELECT ?1, name, sig FROM temp_iattrs;",
		       "D", rowid);
	if (rc != SQLITE_OK) goto out_fail;

	debug("Cache add output attributes\n");
	rc = sql_query(NULL, ocache_DB,
		"INSERT OR REPLACE INTO output_attrs (cache_entry, name, sig)"
		       "  SELECT ?1, name, sig FROM temp_oattrs;",
		       "D", rowid);
	if (rc != SQLITE_OK) goto out_fail;

	if (!if_cache_oattr)
		goto out;

	sql_query(&res, ocache_DB, "SELECT sum(length) FROM temp_oattrs;",NULL);
	if (!res) goto out;

	sql_query_row(res, "D", &oattr_size);
	sql_query_free(res);

	/* if it took a long time to generate a small amount of data, it
	 * should be useful to cache the results so that we can read the
	 * attributes from the cache instead of reexecuting the filter. */
	if ((oattr_size*1000LL) >= (ESTIMATED_ATTR_READ_BW*(int64_t)elapsed_ms))
		goto out;

	debug("Cache add attributes values\n");

	res = NULL;
	rc = sql_query(&res, ocache_DB,
		       "SELECT name, sig FROM temp_oattrs;", NULL);
	while (rc == SQLITE_ROW) {
		char *name;
		sig_val_t *sig, check_sig;
		unsigned char *data;
		size_t len;

		sql_query_row(res, "sb", &name, &sig, &len);
		assert(len == sizeof(sig_val_t));

		/* make sure the attribute still has the same signature */
		odisk_get_attr_sig(obj, name, &check_sig);
		if (memcmp(sig, &check_sig, sizeof(sig_val_t)) != 0) {
		    rc = sql_query_next(res);
		    continue;
		}

		rc = obj_ref_attr(&obj->attr_info, name, &len, &data);
		assert(rc == 0);

		rc = sql_query(NULL, ocache_DB,
			    "INSERT OR IGNORE INTO oattr.attrs"
			    " (name, sig, value) VALUES (?1, ?2, ?3);",
			    "SBB", name, sig, sizeof(sig_val_t), data, len);
		if (rc != SQLITE_OK) break;

		rc = sql_query_next(res);
	}
	sql_query_free(res);
out_fail:
	if (rc != SQLITE_OK)
		sql_rollback(ocache_DB);
out:
	sql_query(NULL, ocache_DB, "DELETE FROM temp_iattrs;", NULL);
	sql_query(NULL, ocache_DB, "DELETE FROM temp_oattrs;", NULL);

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

	debug("ocache_init called\n");

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
