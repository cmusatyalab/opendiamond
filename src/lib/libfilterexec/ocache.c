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

typedef struct {
	unsigned int	name_len;
	char		the_attr_name[MAX_ATTR_NAME];
	sig_val_t	attr_sig;
} cache_attr_entry;

typedef struct {
	unsigned int entry_num;
	cache_attr_entry **entry_data;
} cache_attr_set;

struct cache_obj_s {
	sig_val_t		id_sig;
	sig_val_t		iattr_sig;
	int			result;
	unsigned short		eval_count; //how many times this filter is evaluated
	unsigned short		aeval_count; //how many times this filter is evaluated
	unsigned short		hit_count; //how many times this filter is evaluated
	unsigned short		ahit_count; //how many times this filter is evaluated
	cache_attr_set		iattr;
	cache_attr_set		oattr;
	query_info_t		qid;		// query that created entry
	filter_exec_mode_t	exec_mode;  // exec mode when entry created
	struct cache_obj_s	*next;
};
typedef struct cache_obj_s cache_obj;

struct cache_init_obj_s {
	sig_val_t		id_sig;
	cache_attr_set		attr;
	struct cache_init_obj_s	*next;
};
typedef struct cache_init_obj_s cache_init_obj;



/*
 * dctl variables 
 */
static unsigned int if_cache_table = 1;
static unsigned int if_cache_oattr = 0;

#define OCACHE_DB_NAME "/ocache.db"
static sqlite3 *ocache_DB;

static GAsyncQueue *ocache_queue;

struct ocache_start_entry {
	void *cache_table;	/* ocache */
	sig_val_t fsig;		/* oattr - filter signature */
};

struct ocache_attr_entry {
	obj_data_t *	obj;
	unsigned int	name_len;
	char		name[MAX_ATTR_NAME];
};

struct ocache_end_entry {
	int result;
	query_info_t qid;		/* search that created entry */
	filter_exec_mode_t exec_mode;	/* mode when entry was created */
};

struct ocache_ring_entry {
	int type;
	sig_val_t id_sig;
	union {
		struct ocache_start_entry start;/* INSERT_START */
		cache_attr_entry iattr;		/* INSERT_IATTR */
		struct ocache_attr_entry oattr;	/* INSERT_OATTR */
		struct ocache_end_entry end;	/* INSERT_END */
	} u;
};

static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_FILTER_ARG_NAME 256

static sig_val_t ocache_sig = { {0,} };

/* wrappers around g_async_queue_push and pop to get type checking */
static void ocache_queue_push(struct ocache_ring_entry *data)
{
	g_async_queue_push(ocache_queue, (gpointer)data);
}

static struct ocache_ring_entry *ocache_queue_pop(void)
{
	return (struct ocache_ring_entry *)g_async_queue_pop(ocache_queue);
}

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

/*
 * This is only used in this file, maybe there is a duplicate implementation
 * in another places?
 */
static void
sig_iattr(cache_attr_set * iattr, sig_val_t * sig)
{
	struct ciovec *iov;
	unsigned int i;

	iov = (struct ciovec *)malloc(iattr->entry_num * sizeof(struct ciovec));
	assert(iov != NULL);

	for (i = 0; i < iattr->entry_num; i++) {
		iov[i].iov_base = iattr->entry_data[i];
		iov[i].iov_len = sizeof(cache_attr_entry);
	}

	sig_cal_vec(iov, iattr->entry_num, sig);
	free(iov);
}

static int
ocache_entry_free(cache_obj * cobj)
{
	unsigned int i;

	if (cobj == NULL) {
		return (0);
	}

	for (i = 0; i < cobj->iattr.entry_num; i++) {
		if (cobj->iattr.entry_data[i] != NULL) {
			free(cobj->iattr.entry_data[i]);
		}
	}

	if (cobj->iattr.entry_data != NULL) {
		free(cobj->iattr.entry_data);
	}
	for (i = 0; i < cobj->oattr.entry_num; i++) {
		if (cobj->oattr.entry_data[i] != NULL) {
			free(cobj->oattr.entry_data[i]);
		}
	}
	if (cobj->oattr.entry_data != NULL) {
		free(cobj->oattr.entry_data);
	}
	free(cobj);
	return (0);
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
"    filter_sig  BLOB NOT NULL,"
"    object_sig  BLOB NOT NULL,"
"    iattr_sig   BLOB NOT NULL,"
"    confidence  INTEGER NOT NULL"
"); "
"CREATE INDEX IF NOT EXISTS filter_object_idx ON cache (filter_sig,object_sig);"
""
"CREATE TABLE IF NOT EXISTS attrs ("
"    attr_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
"    name    TEXT NOT NULL,"
"    sig     BLOB NOT NULL,"
"    value   BLOB,"
"    UNIQUE (name, sig)"
");"
""
"CREATE TABLE IF NOT EXISTS input_attrs ("
"    cache_entry INTEGER,"
"    attr_id     INTEGER,"
"    PRIMARY KEY (cache_entry, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX IF NOT EXISTS input_attr_idx ON input_attrs (attr_id);"
""
"CREATE TABLE IF NOT EXISTS output_attrs ("
"    cache_entry INTEGER,"
"    attr_id     INTEGER,"
"    PRIMARY KEY (cache_entry, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX IF NOT EXISTS output_attr_idx ON output_attrs (cache_entry);"
""
"CREATE TABLE IF NOT EXISTS initial_attrs ("
"    object_sig  INTEGER,"
"    attr_id     INTEGER,"
"    PRIMARY KEY (object_sig, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX IF NOT EXISTS initial_attr_idx ON initial_attrs (object_sig);"
""
"CREATE TEMP TABLE current_attrs ("
"    query_id    INTEGER,"
"    attr_id     INTEGER,"
"    PRIMARY KEY (query_id, attr_id) ON CONFLICT IGNORE"
");"
"CREATE INDEX current_attr_idx ON current_attrs (query_id);"
"COMMIT TRANSACTION;"
			, NULL, NULL, &errmsg);
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
"SELECT cache_entry, confidence, iattr_sig FROM cache"
"  WHERE object_sig = ?1 AND filter_sig = ?2 AND"
"  cache_entry NOT IN"
"  (SELECT input_attrs.cache_entry FROM input_attrs, cache"
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

		/* iattr_sig */
		rc = get_column_blob(SQL, 3, iattr_sig, sizeof(sig_val_t));
		if (rc != SQLITE_OK) goto out_fail;

		/* not storing this yet, is it really needed? */
		//rc = get_column_blob(SQL, 4, qinfo, sizeof(query_info_t));

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
cache_combine_attr_set(query_info_t *qid, int cache_entry_hit)
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

	rc = sqlite3_bind_int(SQL, 2, cache_entry_hit);
	if (rc != SQLITE_OK) goto out_fail;

	/* Run query */
	while (1) {
		rc = sqlite3_step(SQL);

		/* Maybe we should sleep, but this should only happen when
		 * another adiskd process is updating the output_attrs table,
		 * which only happens after a filter has been executed (so not
		 * that often) */
		if (rc == SQLITE_BUSY)
			continue;
		break;
	}

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
"INSERT OR IGNORE INTO attrs (name, sig) VALUES (?2, ?3);"
"INSERT OR IGNORE INTO initial_attrs (object_sig, attr_id)"
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
"INSERT OR IGNORE INTO current_attrs (query_id, attr_id)"
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
#warning "should return success only if we actually found initial attributes?"
	return 1;
}

int
ocache_add_start(lf_obj_handle_t ohandle, char *fhandle, void *cache_table,
		 sig_val_t *fsig)
{
	struct ocache_ring_entry *new_entry;
	obj_data_t *obj = (obj_data_t *) ohandle;

	memcpy(&ocache_sig, &obj->id_sig, sizeof(sig_val_t));

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_START;
	memcpy(&new_entry->id_sig, &obj->id_sig, sizeof(sig_val_t));
	memcpy(&new_entry->u.start.fsig, fsig, sizeof(sig_val_t));
	new_entry->u.start.cache_table = cache_table;

	ocache_queue_push(new_entry);
	return 0;
}

static void
ocache_add_iattr(lf_obj_handle_t ohandle,
		 const char *name, off_t len, const unsigned char *data)
{
	struct ocache_ring_entry *new_entry;
	obj_data_t *obj = (obj_data_t *) ohandle;

	if (!sig_match(&ocache_sig, &obj->id_sig))
		return;

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_IATTR;
	memcpy(&new_entry->id_sig, &obj->id_sig, sizeof(sig_val_t));
	new_entry->u.iattr.name_len = name ? strlen(name) + 1 : 0;
	strncpy(new_entry->u.iattr.the_attr_name, name, MAX_ATTR_NAME);
	odisk_get_attr_sig(obj, name, &new_entry->u.iattr.attr_sig);

	ocache_queue_push(new_entry);
}

static void
ocache_add_oattr(lf_obj_handle_t ohandle, const char *name,
		 off_t len, const unsigned char *data)
{
	struct ocache_ring_entry *new_entry;
	obj_data_t *obj = (obj_data_t *) ohandle;

	if (!sig_match(&ocache_sig, &obj->id_sig))
		return;

	/*
	 * call function to update stats 
	 */
	ceval_wattr_stats(len);

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_OATTR;
	memcpy(&new_entry->id_sig, &obj->id_sig, sizeof(sig_val_t));
	new_entry->u.oattr.obj = obj;
	new_entry->u.oattr.name_len = name ? strlen(name) + 1 : 0;
	strncpy(new_entry->u.oattr.name, name, MAX_ATTR_NAME);

	odisk_ref_obj(obj);

	ocache_queue_push(new_entry);
}

int
ocache_add_end(lf_obj_handle_t ohandle, char *fhandle, int conf,
	       query_info_t *qid, filter_exec_mode_t exec_mode)
{
	struct ocache_ring_entry *new_entry;
	obj_data_t *obj = (obj_data_t *) ohandle;

	if (!sig_match(&ocache_sig, &obj->id_sig))
		return 0;

	new_entry = (struct ocache_ring_entry *) calloc(1, sizeof(*new_entry));
	assert(new_entry != NULL);

	new_entry->type = INSERT_END;
	memcpy(&new_entry->id_sig, &obj->id_sig, sizeof(sig_val_t));
	new_entry->u.end.result = conf;
	new_entry->u.end.qid = *qid;
	new_entry->u.end.exec_mode = exec_mode;

	sig_clear(&ocache_sig);

	ocache_queue_push(new_entry);
	return 0;
}

#define	MAX_VEC_SIZE	10

static void    *
ocache_main(void *arg)
{
	ocache_state_t *cstate = (ocache_state_t *) arg;
	struct ocache_ring_entry *tobj;

	/* ocache objects */
	cache_obj      *cobj;
	int             correct;
	cache_obj     **cache_table;
	cache_attr_entry **tmp, *src_attr, attr;
	cache_attr_set *attr_set;

	/* oattr objects */
	char *s_str, *i_str, attrbuf[PATH_MAX], new_attrbuf[PATH_MAX];
	int             fd;
	struct iovec    wvec[MAX_VEC_SIZE];
	obj_data_t     *ovec[MAX_VEC_SIZE];
	int             i, wcount, err;

	while (1) {
		/*
		 * get the next lookup object 
		 */
		tobj = ocache_queue_pop();
		if (tobj->type != INSERT_START) {
			if (tobj->type == INSERT_OATTR)
				odisk_release_obj(tobj->u.oattr.obj);
			free(tobj);
			continue;
		}

		/*
		 * for one thread case, we could do it in this simple way.
		 * XXX: do we need to change this later?
		 */
		correct = 0;
		cache_table = NULL;
		fd = -1;
		attrbuf[0] = '\0';

		/* ocache */
		cobj = (cache_obj *) calloc(1, sizeof(*cobj));
		assert(cobj != NULL);
		memcpy(&cobj->id_sig, &tobj->id_sig, sizeof(sig_val_t));
		cobj->aeval_count = 1;
		cobj->ahit_count = 1;
		if (if_cache_table)
			cache_table = tobj->u.start.cache_table;

		/* oattr */
		s_str = sig_string(&tobj->u.start.fsig);
		i_str = sig_string(&tobj->id_sig);
		assert(s_str != NULL && i_str != NULL);

		if (if_cache_oattr) {
			sprintf(attrbuf, "%s/%s/%s", cstate->ocache_path,
				s_str, i_str);
			fd = open(attrbuf, O_WRONLY | O_CREAT | O_EXCL, 0777);
			if (fd == -1) {
				if (errno != EEXIST)
					printf("failed to open %s \n", attrbuf);
			}
			else if (flock(fd, LOCK_EX) != 0) {
				perror("error locking oattr file\n");
				close(fd);
				fd = -1;
			}
		}

		free(s_str);
		free(i_str);

		wcount = 0;
		free(tobj);

		while (1) {
			tobj = ocache_queue_pop();

			if (!sig_match(&cobj->id_sig, &tobj->id_sig) ||
			    tobj->type == INSERT_START)
			{
				if (tobj->type == INSERT_OATTR)
					odisk_release_obj(tobj->u.oattr.obj);
				free(tobj);
				break;
			}

			if (tobj->type == INSERT_END) {
				cobj->result = tobj->u.end.result;
				cobj->qid = tobj->u.end.qid;
				cobj->exec_mode = tobj->u.end.exec_mode;
				correct = 1;
				free(tobj);
				break;
			}

			/* tobj->type == INSERT_IATTR or INSERT_OATTR */

			if (tobj->type == INSERT_OATTR) {
				attr_record_t *arec;

				memset(&attr, 0, sizeof(attr));
				attr.name_len = tobj->u.oattr.name_len;
				strncpy(attr.the_attr_name, tobj->u.oattr.name,
					MAX_ATTR_NAME);
				odisk_get_attr_sig(tobj->u.oattr.obj,
						   tobj->u.oattr.name,
						   &attr.attr_sig);

				arec = odisk_get_arec(tobj->u.oattr.obj,
						      tobj->u.oattr.name);
				if (arec) {
					wvec[wcount].iov_base = arec;
					wvec[wcount].iov_len = arec->rec_len;
					ovec[wcount] = tobj->u.oattr.obj;
					wcount++;
				} else
					odisk_release_obj(tobj->u.oattr.obj);

				src_attr = &attr;
				attr_set = &cobj->oattr;
			} else {
				src_attr = &tobj->u.iattr;
				attr_set = &cobj->iattr;
			}

			free(tobj);

			if ((attr_set->entry_num % ATTR_ENTRY_NUM) == 0) {
				tmp = calloc(attr_set->entry_num +
					     ATTR_ENTRY_NUM,
					     sizeof(cache_attr_entry *));
				assert(tmp != NULL);
				if (attr_set->entry_data != NULL) {
					memcpy(tmp, attr_set->entry_data,
					       attr_set->entry_num *
					       sizeof(cache_attr_entry *));
					free(attr_set->entry_data);
				}
				attr_set->entry_data = tmp;
			}

			tmp = &attr_set->entry_data[attr_set->entry_num];
			*tmp = calloc(1, sizeof(cache_attr_entry));
			assert(*tmp != NULL);
			memcpy(*tmp, src_attr, sizeof(cache_attr_entry));
			attr_set->entry_num++;

			/* if the oattr write vector is full then flush it */
			if (wcount == MAX_VEC_SIZE) {
				if (fd != -1) {
					err = writev(fd, wvec, wcount);
					assert(err >= 0);
				}
				for (i = 0; i < wcount; i++)
					odisk_release_obj(ovec[i]);
				wcount = 0;
			}
		}

		/* flush and release remaining oattr cache entries */
		if (fd != -1) {
			err = writev(fd, wvec, wcount);
			assert(err >= 0);
			close(fd);
			fd = -1;
		}
		for (i = 0; i < wcount; i++)
			odisk_release_obj(ovec[i]);

		sig_iattr(&cobj->iattr, &cobj->iattr_sig);

		if (fd != -1) {
			if (correct) {
				s_str = sig_string(&cobj->iattr_sig);
				assert(s_str != NULL);
				sprintf(new_attrbuf, "%s.%s", attrbuf, s_str);
				free(s_str);
				rename(attrbuf, new_attrbuf);
			} else
				unlink(attrbuf);
		}
#if 0
		if (cache_table && correct) {
			unsigned int index;
			index = sig_hash(&cobj->id_sig) % CACHE_ENTRY_NUM;
			pthread_mutex_lock(&shared_mutex);
			cobj->next = cache_table[index];
			cache_table[index] = cobj;
			cache_entry_num++;
			pthread_mutex_unlock(&shared_mutex);
		} else
#endif
			ocache_entry_free(cobj);

#if 0
		if (cache_entry_num >= MAX_CACHE_ENTRY_NUM)
			free_fcache_entry(cstate->ocache_path);
#endif
	}
}


int
ocache_init(char *dirp)
{
	ocache_state_t *new_state;
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

	if (!g_thread_supported()) g_thread_init(NULL);

	/*
	 * dctl control 
	 */
	dctl_register_leaf(DEV_CACHE_PATH, "cache_table", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &if_cache_table);
	dctl_register_leaf(DEV_CACHE_PATH, "cache_oattr", DCTL_DT_UINT32,
			   dctl_read_uint32, dctl_write_uint32,
			   &if_cache_oattr);

	ocache_queue = g_async_queue_new();

	new_state = (ocache_state_t *) calloc(1, sizeof(*new_state));
	assert(new_state != NULL);

	/*
	 * set callback functions so we get notifice on read/and writes
	 * to object attributes.
	 */
	lf_set_read_cb(ocache_add_iattr);
	lf_set_write_cb(ocache_add_oattr);

	/*
	 * the length has already been tested above 
	 */
	strcpy(new_state->ocache_path, dir_path);

	/*
	 * create thread to process inserted entries for cache table 
	 */
	err = pthread_create(&new_state->c_thread_id, NULL,
			     ocache_main, (void *) new_state);

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

