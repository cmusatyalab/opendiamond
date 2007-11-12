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

#include <sqlite3.h>

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
sql_error(const char *f, sqlite3 *db, const char *sql)
{
	char frag[32]; strncpy(frag, sql, 31); frag[31] = '\0';
	fprintf(stderr, "%s: %s at '%s'\n", f, sqlite3_errmsg(db), frag);
}

static int
sql_prepare_multi(const char *f, sqlite3 *db, sqlite3_stmt **mSQL,
		  const char *zSql)
{
	int rc, i = 0;

	while (1) {
		rc = sqlite3_prepare_v2(db, zSql, -1, &mSQL[i], &zSql);
		if (rc != SQLITE_OK) goto err;
		if (mSQL[i] == NULL) continue;
		if (*zSql == '\0') break;
	}
	mSQL[i] = NULL; /* add a NULL at the end of the array */
	return SQLITE_OK;
err:
	sql_error(f, db, zSql);
	for (; i >= 0; i--) {
	    sqlite3_finalize(mSQL[i]);
	    mSQL[i] = NULL;
	}
	return rc;
}

static void
sql_step_multi(const char *f, sqlite3_stmt **mSQL)
{
	int i, rc = SQLITE_ERROR;
	for (i = 0; mSQL[i] != NULL;) {
		/* Run query */
		rc = sqlite3_step(mSQL[i]);

		/* Maybe we should sleep, but this should only happen when
		 * another adiskd process is updating the output_attrs table,
		 * which only happens after a filter has been executed (so not
		 * that often) */
		if (rc == SQLITE_BUSY) {
			fprintf(stderr, "sqlite busy, retrying\n");
			/* sqlite_sleep(1); */
			continue;
		}

		if (rc == SQLITE_ROW)
			continue;

		sqlite3_reset(mSQL[i]);

		if (rc != SQLITE_DONE)
			break;

		i++;
	}
	if (rc != SQLITE_DONE)
		sql_error(f, sqlite3_db_handle(mSQL[i]), "[sql_step_multi]");
}

#if 0
static int
get_column_blob(sqlite3_stmt *stmt, int col, void *blob, int size)
{
	/* call sqlite3_column_blob before calling sqlite3_column_bytes */
	const void *p = sqlite3_column_blob(stmt, col);

	if (size != sqlite3_column_bytes(stmt, col))
		return SQLITE_TOOBIG;

	memcpy(blob, p, size);
	return SQLITE_OK;
}
#endif

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
"    create_time TEXT DEFAULT CURRENT_TIMESTAMP,"
/*"  create_time INTEGER " -- DEFAULT strftime("%s", "now", "utc") */
"    filter_sig	BLOB NOT NULL,"
"    object_sig	BLOB NOT NULL,"
"    confidence	INTEGER NOT NULL"
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
"CREATE INDEX IF NOT EXISTS input_attr_idx ON input_attrs (attr_id);"
""
"CREATE TABLE IF NOT EXISTS output_attrs ("
"    cache_entry INTEGER NOT NULL,"
"    attr_id     INTEGER NOT NULL,"
"    PRIMARY KEY (cache_entry, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX IF NOT EXISTS output_attr_idx ON output_attrs (cache_entry);"
""
"CREATE TABLE IF NOT EXISTS initial_attrs ("
"    object_sig INTEGER NOT NULL,"
"    attr_id    INTEGER NOT NULL,"
"    PRIMARY KEY (object_sig, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX IF NOT EXISTS initial_attr_idx ON initial_attrs (object_sig);"
""
"CREATE TEMP TABLE current_attrs ("
"    query_id   INTEGER NOT NULL,"
"    attr_id    INTEGER NOT NULL,"
"    PRIMARY KEY (query_id, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX current_attr_idx ON current_attrs (query_id);"
""
"CREATE TEMP TABLE temp_iattrs ("
"    query_id   INTEGER NOT NULL,"
"    attr_id    INTEGER NOT NULL,"
"    UNIQUE (query_id, attr_id) ON CONFLICT IGNORE"
");"
"CREATE TEMP TABLE temp_oattrs ("
"    query_id   INTEGER NOT NULL,"
"    name	TEXT NOT NULL,"
"    sig	BLOB NOT NULL,"
"    value      BLOB,"
"    UNIQUE (query_id, name) ON CONFLICT REPLACE"
");"
"COMMIT TRANSACTION;" , NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
	    fprintf(stderr, "ocache.db initialization failed: %s\n", errmsg);
	    sqlite3_free(errmsg);
	    ocache_DB = NULL;
    }
    debug("done\n");
}

int
cache_lookup(sig_val_t *idsig, sig_val_t *fsig, query_info_t *qid,
	     int *err, int *cache_entry_hit, sig_val_t *iattr_sig)
{
	static sqlite3_stmt *SQL = NULL;
	const char *tail;
	int rc, found = 0;

	if (if_cache_table == 0 || ocache_DB == NULL)
		return 0;

	sig_clear(iattr_sig);

	pthread_mutex_lock(&shared_mutex);
	debug("Cache lookup\n");

	if (!SQL) {
		rc = sqlite3_prepare_v2(ocache_DB,
"SELECT cache_entry, confidence FROM cache"
"  WHERE object_sig = ?1 AND filter_sig = ?2 AND"
"  cache_entry NOT IN"
"  (SELECT input_attrs.cache_entry FROM cache, input_attrs"
"     WHERE cache.cache_entry = input_attrs.cache_entry AND"
"     cache.object_sig = ?1 AND cache.filter_sig = ?2 AND"
"     input_attrs.attr_id NOT IN"
"     (SELECT attr_id FROM current_attrs WHERE query_id = ?3))"
"  LIMIT 1;", -1, &SQL, &tail);
		if (rc != SQLITE_OK || SQL == NULL) {
			sql_error(__func__, ocache_DB, tail);
			if (SQL) sqlite3_finalize(SQL);
			SQL = NULL;
			goto out_unlock;
		}
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_blob(SQL, 1, idsig, sizeof(sig_val_t), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sqlite3_bind_blob(SQL, 2, fsig, sizeof(sig_val_t), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sqlite3_bind_int(SQL, 3, qid->query_id);
	if (rc != SQLITE_OK) goto out_fail;

	/* Run query */
	rc = sqlite3_step(SQL);
	if (rc == SQLITE_ROW) {
		/* handle to find the output attr set when we're extending the
		 * current_attr set */
		*cache_entry_hit = sqlite3_column_int(SQL, 1);

		/* confidence value */
		*err = sqlite3_column_int(SQL, 2);

		found = 1;
	}

out_fail:
	/* Because we defined the bound values as static, sqlite3 didn't make a
	 * private copy and we have to clear them before returning */
	sqlite3_reset(SQL);
	sqlite3_clear_bindings(SQL);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);

	return found;
}

void
cache_combine_attr_set(query_info_t *qid, int cache_entry)
{
	static sqlite3_stmt *SQL = NULL;
	const char *tail;
	int rc;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache combine attr set\n");

	if (!SQL) {
		rc = sqlite3_prepare_v2(ocache_DB,
"INSERT INTO current_attrs (query_id, attr_id)"
"  SELECT ?1, attr_id FROM output_attrs"
"  WHERE cache_entry = ?2;", -1, &SQL, &tail);
		if (rc != SQLITE_OK || SQL == NULL) {
			sql_error(__func__, ocache_DB, tail);
			if (SQL) sqlite3_finalize(SQL);
			SQL = NULL;
			goto out_unlock;
		}
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_int(SQL, 1, qid->query_id);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sqlite3_bind_int(SQL, 2, cache_entry);
	if (rc != SQLITE_OK) goto out_fail;

	/* Run query */
	do { rc = sqlite3_step(SQL);
	} while (rc == SQLITE_BUSY);

out_fail:
	sqlite3_reset(SQL);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);
}

/* Build state to keep track of initial object attributes. */
void
cache_set_init_attrs(sig_val_t *idsig, obj_attr_t *init_attr)
{
	static sqlite3_stmt *mSQL[10] = { NULL, };
	int rc;
	unsigned char *buf;
	size_t len;
	void *cookie;
	attr_record_t *arec;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache set init attrs\n");

	if (mSQL[0] == NULL) {
		rc = sql_prepare_multi(__func__, ocache_DB, mSQL,
"BEGIN TRANSACTION;"
"INSERT INTO attrs (name, sig) VALUES (?2, ?3);"
"INSERT INTO initial_attrs (object_sig, attr_id)"
"  SELECT ?1, attr_id FROM attrs WHERE name = ?2 AND sig = ?3;"
"COMMIT TRANSACTION;");
		if (rc != SQLITE_OK)
			goto out_unlock;
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_blob(mSQL[2], 1, idsig, sizeof(sig_val_t), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_unlock;

	rc = obj_get_attr_first(init_attr, &buf, &len, &cookie, 0);
	while (rc != ENOENT) {
		if (buf == NULL) {
			printf("can not get attr\n");
			break;
		}
		arec = (attr_record_t *) buf;

		rc = sqlite3_bind_text(mSQL[1], 2, (char *)arec->data,
				       arec->name_len, SQLITE_STATIC);
		if (rc != SQLITE_OK) goto bind_fail;
		rc = sqlite3_bind_text(mSQL[2], 2, (char *)arec->data,
				       arec->name_len, SQLITE_STATIC);
		if (rc != SQLITE_OK) goto bind_fail;

		rc = sqlite3_bind_blob(mSQL[1], 3, &arec->attr_sig,
				       sizeof(sig_val_t), SQLITE_STATIC);
		if (rc != SQLITE_OK) goto bind_fail;
		rc = sqlite3_bind_blob(mSQL[2], 3, &arec->attr_sig,
				       sizeof(sig_val_t), SQLITE_STATIC);
		if (rc != SQLITE_OK) goto bind_fail;

		/* Run query */
		sql_step_multi(__func__, mSQL);
bind_fail:
		rc = obj_get_attr_next(init_attr, &buf, &len, &cookie, 0);
	}
	sqlite3_clear_bindings(mSQL[1]);
	sqlite3_clear_bindings(mSQL[2]);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);
}

int
cache_get_init_attrs(query_info_t *qid, sig_val_t *idsig)
{
	static sqlite3_stmt *mSQL[10] = { NULL, };
	int rc;

	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache get init attrs\n");

	if (mSQL[0] == NULL) {
		rc = sql_prepare_multi(__func__, ocache_DB, mSQL,
"BEGIN TRANSACTION;"
"DELETE FROM current_attrs WHERE query_id = ?1;"
"INSERT INTO current_attrs (query_id, attr_id)"
"  SELECT ?1, attr_id FROM initial_attrs WHERE object_sig = ?2;"
"COMMIT TRANSACTION;");
		if (rc != SQLITE_OK)
			goto out_unlock;
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_int(mSQL[1], 1, qid->query_id);
	if (rc != SQLITE_OK) goto out_fail;
	rc = sqlite3_bind_int(mSQL[2], 1, qid->query_id);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sqlite3_bind_blob(mSQL[2], 2, idsig, sizeof(sig_val_t), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	/* Run query */
	sql_step_multi(__func__, mSQL);
out_fail:
	/* we need to clear any SQLITE_STATIC bindings */
	sqlite3_clear_bindings(mSQL[2]);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);
	return 1;
}

int
ocache_add_start(lf_obj_handle_t ohandle, sig_val_t *fsig)
{
	/* check if we don't have temp_iattr/temp_oattr entries for this
	 * object? */
	return 0;
}

static void
ocache_add_iattr(lf_obj_handle_t ohandle,
		 const char *name, off_t len, const unsigned char *data)
{
	static sqlite3_stmt *SQL = NULL;
	const char *tail;
	int rc;
	obj_data_t *obj = (obj_data_t *) ohandle;
	sig_val_t sig;

	/* don't have a query id here, no concurrent filter execution */
	int query_id = 0;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache insert temporary iattr '%s'\n", name);

	if (!SQL) {
		rc = sqlite3_prepare_v2(ocache_DB,
"INSERT INTO temp_iattrs (query_id, attr_id)"
"  SELECT ?1, attr_id FROM current_attrs, attrs USING (attr_id)"
"  WHERE attrs.name = ?2 AND attrs.sig = ?3;", -1, &SQL, &tail);
		if (rc != SQLITE_OK || SQL == NULL) {
			sql_error(__func__, ocache_DB, tail);
			if (SQL) sqlite3_finalize(SQL);
			SQL = NULL;
			goto out_unlock;
		}
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_int(SQL, 1, query_id);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sqlite3_bind_text(SQL, 2, name, strlen(name), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	odisk_get_attr_sig(obj, name, &sig);
	rc = sqlite3_bind_blob(SQL, 3, &sig, sizeof(sig_val_t), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	/* Run query */
	do { rc = sqlite3_step(SQL);
	} while (rc == SQLITE_BUSY);

out_fail:
	sqlite3_reset(SQL);
	sqlite3_clear_bindings(SQL);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);
}

static void
ocache_add_oattr(lf_obj_handle_t ohandle, const char *name,
		 off_t len, const unsigned char *data)
{
	static sqlite3_stmt *SQL = NULL;
	const char *tail;
	int rc;
	obj_data_t *obj = (obj_data_t *) ohandle;
	sig_val_t sig;

	/* don't have a query id here, no concurrent filter execution */
	int query_id = 0;

	if (!if_cache_table || ocache_DB == NULL)
		return;

	/* call function to update stats */
	ceval_wattr_stats(len);

	pthread_mutex_lock(&shared_mutex);
	debug("Cache insert temporary oattr '%s'\n", name);

	if (!SQL) {
		rc = sqlite3_prepare_v2(ocache_DB,
"INSERT INTO temp_oattrs (query_id, name, sig, value) VALUES (?1, ?2, ?3, ?4);"
					, -1, &SQL, &tail);
		if (rc != SQLITE_OK || SQL == NULL) {
			sql_error(__func__, ocache_DB, tail);
			if (SQL) sqlite3_finalize(SQL);
			SQL = NULL;
			goto out_unlock;
		}
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_int(SQL, 1, query_id);
	if (rc != SQLITE_OK) goto out_fail;

	rc = sqlite3_bind_text(SQL, 2, name, strlen(name), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	odisk_get_attr_sig(obj, name, &sig);
	rc = sqlite3_bind_blob(SQL, 3, &sig, sizeof(sig_val_t), SQLITE_STATIC);
	if (rc != SQLITE_OK) goto out_fail;

	if (if_cache_oattr && data != NULL /* && len < magic_value */) {
		rc = sqlite3_bind_blob(SQL, 4, data, len, SQLITE_STATIC);
		if (rc != SQLITE_OK) goto out_fail;
	}

	/* Run query */
	do { rc = sqlite3_step(SQL);
	} while (rc == SQLITE_BUSY);

out_fail:
	sqlite3_reset(SQL);
	sqlite3_clear_bindings(SQL);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);
}

int
ocache_add_end(lf_obj_handle_t ohandle, sig_val_t *fsig, int conf,
	       query_info_t *qinfo, filter_exec_mode_t exec_mode)
{
	static sqlite3_stmt *mSQL[10] = { NULL, };
	int rc;
	obj_data_t *obj = (obj_data_t *) ohandle;
	sqlite_int64 rowid;

	/* although we have a query id here we didn't when we added the
	 * temporary input and output attributes */
	int query_id = 0;

	if (!if_cache_table || ocache_DB == NULL)
		return 0;

	pthread_mutex_lock(&shared_mutex);
	debug("Cache add filter results\n");

	if (mSQL[0] == NULL) {
		rc = sql_prepare_multi(__func__, ocache_DB, mSQL,
"BEGIN TRANSACTION;"
"INSERT INTO attrs (name, sig, value)"
"  SELECT name, sig, value FROM temp_oattrs WHERE query_id = ?1;"
"INSERT INTO cache (filter_sig, object_sig, confidence)"
"  VALUES (?2, ?3, ?4);"
"INSERT INTO output_attrs (cache_entry, attr_id)"
"  SELECT cache_entry, attr_id FROM cache, temp_oattrs, attrs USING(name, sig)"
"  WHERE filter_sig = ?2 AND object_sig = ?3 AND query_id = ?1;"
"INSERT INTO input_attrs (cache_entry, attr_id)"
"  SELECT cache_entry, attr_id FROM cache, temp_iattrs"
"  WHERE filter_sig = ?2 AND object_sig = ?3 AND query_id = ?1;"
"DELETE FROM temp_oattrs WHERE query_id = ?1;"
"DELETE FROM temp_iattrs WHERE query_id = ?1;"
"COMMIT TRANSACTION;");
		if (rc != SQLITE_OK)
			goto out_unlock;
	}

	/* Bind values to the prepared statement */
	rc = sqlite3_bind_int(mSQL[1], 1, query_id);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_int(mSQL[3], 1, query_id);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_int(mSQL[4], 1, query_id);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_int(mSQL[5], 1, query_id);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_int(mSQL[6], 1, query_id);
	if (rc != SQLITE_OK) goto bind_fail;

	rc = sqlite3_bind_blob(mSQL[2], 2, fsig, sizeof(sig_val_t),
			       SQLITE_STATIC);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_blob(mSQL[3], 2, fsig, sizeof(sig_val_t),
			       SQLITE_STATIC);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_blob(mSQL[4], 2, fsig, sizeof(sig_val_t),
			       SQLITE_STATIC);
	if (rc != SQLITE_OK) goto bind_fail;

	rc = sqlite3_bind_blob(mSQL[2], 3, &obj->id_sig, sizeof(sig_val_t),
			       SQLITE_STATIC);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_blob(mSQL[3], 3, &obj->id_sig, sizeof(sig_val_t),
			       SQLITE_STATIC);
	if (rc != SQLITE_OK) goto bind_fail;
	rc = sqlite3_bind_blob(mSQL[4], 3, &obj->id_sig, sizeof(sig_val_t),
			       SQLITE_STATIC);
	if (rc != SQLITE_OK) goto bind_fail;

	rc = sqlite3_bind_int(mSQL[2], 4, conf);
	if (rc != SQLITE_OK) goto bind_fail;

	/* Add input/output attributes */
	sql_step_multi(__func__, mSQL);
bind_fail:
	sqlite3_clear_bindings(mSQL[1]);
	sqlite3_clear_bindings(mSQL[3]);
	sqlite3_clear_bindings(mSQL[4]);
out_unlock:
	pthread_mutex_unlock(&shared_mutex);
	return 0;
}

#define	MAX_VEC_SIZE	10

int
ocache_init(char *dirp)
{
	int             err;
	char           *dir_path;

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

