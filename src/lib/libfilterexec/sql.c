/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 4
 *
 *  Copyright (C) 2006-2007 Carnegie Mellon University
 *  All Rights Reserved
 *
 *  This software is distributed under the terms of the Eclipse Public
 *  License, Version 1.0 which can be found in the file named LICENSE.
 *  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
 *  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
 */

/*
 * SQLite3 helper functions from
 *
 * Parcelkeeper - support daemon for the OpenISR (TM) system virtual disk
 *
 * Copyright (C) 2006-2007 Carnegie Mellon University
 */

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "sql.h"

void sql_query_free(sqlite3_stmt *stmt)
{
	if (stmt)
		sqlite3_finalize(stmt);
}

int sql_query(sqlite3_stmt **result, sqlite3 *db, const char *query,
	      const char *fmt, ...)
{
	sqlite3_stmt *stmt;
	va_list ap;
	int i=1;
	int ret;
	void *blob;

	if (result != NULL)
		*result=NULL;
	ret=sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
	if (ret) {
		fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
		return ret;
	}
	va_start(ap, fmt);
	for (; fmt != NULL && *fmt; fmt++) {
		switch (*fmt) {
		case 'd':
			ret=sqlite3_bind_int(stmt, i++, va_arg(ap, int));
			break;
		case 'D':
			ret=sqlite3_bind_int64(stmt, i++, va_arg(ap, int64_t));
			break;
		case 'f':
			ret=sqlite3_bind_double(stmt, i++, va_arg(ap, double));
			break;
		case 's':
		case 'S':
			ret=sqlite3_bind_text(stmt, i++, va_arg(ap, char *), -1,
					      *fmt == 's'
					      ? SQLITE_TRANSIENT
					      : SQLITE_STATIC);
			break;
		case 'b':
		case 'B':
			blob=va_arg(ap, void *);
			ret=sqlite3_bind_blob(stmt, i++, blob, va_arg(ap, int),
					      *fmt == 'b'
					      ? SQLITE_TRANSIENT
					      : SQLITE_STATIC);
			break;
		default:
			fprintf(stderr, "Unknown format specifier %c\n", *fmt);
			ret=SQLITE_MISUSE;
			break;
		}
		if (ret)
			break;
	}
	va_end(ap);
	if (ret == SQLITE_OK)
		ret=sql_query_next(stmt);
	if (ret != SQLITE_ROW || result == NULL)
		sql_query_free(stmt);
	else
		*result=stmt;
	return ret;
}

int sql_query_next(sqlite3_stmt *stmt)
{
	sqlite3 *db;
	int ret;

	ret=sqlite3_step(stmt);
	if (ret == SQLITE_DONE)
		ret=SQLITE_OK;
	if (ret != SQLITE_OK && ret != SQLITE_ROW) {
		db = sqlite3_db_handle(stmt);
		fprintf(stderr, "SQL error: %s while executing\n\t%s\n",
			sqlite3_errmsg(db), sqlite3_sql(stmt));
	}
	return ret;
}

void sql_query_row(sqlite3_stmt *stmt, const char *fmt, ...)
{
	va_list ap;
	int i=0;

	va_start(ap, fmt);
	for (; *fmt; fmt++) {
		switch (*fmt) {
		case 'd':
			*va_arg(ap, int *)=sqlite3_column_int(stmt, i++);
			break;
		case 'D':
			*va_arg(ap, int64_t *)=sqlite3_column_int64(stmt, i++);
			break;
		case 'f':
			*va_arg(ap, double *)=sqlite3_column_double(stmt, i++);
			break;
		case 's':
		case 'S':
			*va_arg(ap, const unsigned char **)=
						sqlite3_column_text(stmt, i);
			if (*fmt == 'S')
				*va_arg(ap,int *)=sqlite3_column_bytes(stmt, i);
			i++;
			break;
		case 'b':
			*va_arg(ap, const void **)=sqlite3_column_blob(stmt, i);
			*va_arg(ap, int *)=sqlite3_column_bytes(stmt, i++);
			break;
		case 'n':
			*va_arg(ap, int *)=sqlite3_column_bytes(stmt, i++);
			break;
		default:
			fprintf(stderr, "Unknown format specifier %c\n", *fmt);
			break;
		}
	}
	va_end(ap);
}

int sql_attach(sqlite3 *db, const char *handle, const char *file)
{
	int ret = sql_query(NULL, db, "ATTACH ? AS ?", "ss", file, handle);
	if (ret != SQLITE_OK)
		fprintf(stderr, "Couldn't attach %s\n", file);
	return ret;
}

int _sql_begin(const char *caller, sqlite3 *db)
{
	int ret = sql_query(NULL, db, "BEGIN TRANSACTION", NULL);
	if (ret != SQLITE_OK)
		fprintf(stderr, "Couldn't begin transaction for %s\n", caller);
	return ret;
}

int _sql_commit(const char *caller, sqlite3 *db)
{
	int ret = sql_query(NULL, db, "COMMIT", NULL);
	if (ret != SQLITE_OK)
		fprintf(stderr, "Couldn't commit transaction for %s\n", caller);
	return ret;
}

int _sql_rollback(const char *caller, sqlite3 *db)
{
	int ret = sql_query(NULL, db, "ROLLBACK", NULL);
	if (ret != SQLITE_OK)
		fprintf(stderr, "Couldn't roll back transaction for %s\n",
			caller);
	return ret;
}

static int busy_handler(void *db, int count)
{
	int ms;
	(void)db;  /* silence warning */

	if (count > 10) /* don't wait longer than a second. */
		count = 10;

	ms = 1 << count;
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };

	nanosleep(&ts, NULL);
	return 1;
}

int sql_set_busy_handler(sqlite3 *db)
{
	int ret = sqlite3_busy_handler(db, busy_handler, db);
	if (ret != SQLITE_OK)
		fprintf(stderr, "Couldn't set busy handler for database");
	return ret;
}

/* This validates both the primary and attached databases */
int sql_validate_db(sqlite3 *db)
{
	sqlite3_stmt *stmt = NULL;
	const char *str;
	int result;

	if (sql_query(&stmt, db, "PRAGMA integrity_check(1)", NULL) != SQLITE_ROW) {
		sql_query_free(stmt);
		fprintf(stderr, "Couldn't run SQLite integrity check");
		return SQLITE_ERROR;
	}
	sql_query_row(stmt, "s", &str);
	result=strcmp(str, "ok");
	sql_query_free(stmt);
	if (result) {
		fprintf(stderr, "SQLite integrity check failed");
		return SQLITE_CORRUPT;
	}
	return SQLITE_OK;
}

