/*
 * Parcelkeeper - support daemon for the OpenISR (TM) system virtual disk
 *
 * Copyright (C) 2006-2007 Carnegie Mellon University
 *
 * This software is distributed under the terms of the Eclipse Public License,
 * Version 1.0 which can be found in the file named LICENSE.Eclipse.  ANY USE,
 * REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES RECIPIENT'S
 * ACCEPTANCE OF THIS AGREEMENT
 */

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sqlite3.h>
#include "defs.h"

static void sqlerr(sqlite3 *db)
{
	pk_log(LOG_ERROR, "SQL error: %s", sqlite3_errmsg(db));
}

void query_free(sqlite3_stmt *stmt)
{
	if (stmt == NULL)
		return;
	sqlite3_finalize(stmt);
}

int query(sqlite3_stmt **result, sqlite3 *db, char *query, char *fmt, ...)
{
	sqlite3_stmt *stmt;
	va_list ap;
	int i=1;
	int ret;
	int found_unknown=0;
	void *blob;

	if (result != NULL)
		*result=NULL;
	ret=sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
	if (ret) {
		sqlerr(db);
		return ret;
	}
	va_start(ap, fmt);
	for (; fmt != NULL && *fmt; fmt++) {
		switch (*fmt) {
		case 'd':
			ret=sqlite3_bind_int(stmt, i++, va_arg(ap, int));
			break;
		case 'f':
			ret=sqlite3_bind_double(stmt, i++, va_arg(ap, double));
			break;
		case 's':
		case 'S':
			ret=sqlite3_bind_text(stmt, i++, va_arg(ap, char *),
						-1, *fmt == 's'
						? SQLITE_TRANSIENT
						: SQLITE_STATIC);
			break;
		case 'b':
		case 'B':
			blob=va_arg(ap, void *);
			ret=sqlite3_bind_blob(stmt, i++, blob, va_arg(ap, int),
						*fmt == 'b' ? SQLITE_TRANSIENT
						: SQLITE_STATIC);
			break;
		default:
			pk_log(LOG_ERROR, "Unknown format specifier %c", *fmt);
			ret=SQLITE_MISUSE;
			/* Don't call sqlerr(), since we synthesized this
			   error */
			found_unknown=1;
			break;
		}
		if (ret)
			break;
	}
	va_end(ap);
	if (ret == SQLITE_OK)
		ret=sqlite3_step(stmt);
	/* Collapse DONE into OK, since we don't want everyone to have to test
	   for a gratuitously nonzero error code */
	if (ret == SQLITE_DONE)
		ret=SQLITE_OK;
	if (ret != SQLITE_OK && ret != SQLITE_ROW && !found_unknown)
		sqlerr(db);
	if ((ret != SQLITE_OK && ret != SQLITE_ROW) || result == NULL)
		query_free(stmt);
	else
		*result=stmt;
	return ret;
}

int query_next(sqlite3_stmt *stmt)
{
	int ret;

	ret=sqlite3_step(stmt);
	if (ret == SQLITE_DONE)
		ret=SQLITE_OK;
	if (ret != SQLITE_OK && ret != SQLITE_ROW)
		sqlerr(sqlite3_db_handle(stmt));
	return ret;
}

void query_row(sqlite3_stmt *stmt, char *fmt, ...)
{
	va_list ap;
	int i=0;

	va_start(ap, fmt);
	for (; *fmt; fmt++) {
		switch (*fmt) {
		case 'd':
			*va_arg(ap, int *)=sqlite3_column_int(stmt, i++);
			break;
		case 'f':
			*va_arg(ap, double *)=sqlite3_column_double(stmt, i++);
			break;
		case 's':
		case 'S':
			*va_arg(ap, const unsigned char **)=
						sqlite3_column_text(stmt, i);
			if (*fmt == 'S')
				*va_arg(ap, int *)=sqlite3_column_bytes(stmt,
							i);
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
			pk_log(LOG_ERROR, "Unknown format specifier %c", *fmt);
			break;
		}
	}
	va_end(ap);
}

pk_err_t attach(sqlite3 *db, const char *handle, const char *file)
{
	if (query(NULL, db, "ATTACH ? AS ?", "ss", file, handle)) {
		pk_log(LOG_ERROR, "Couldn't attach %s", file);
		return PK_IOERR;
	}
	return PK_SUCCESS;
}

pk_err_t _begin(sqlite3 *db, const char *caller)
{
	if (query(NULL, db, "BEGIN TRANSACTION", NULL)) {
		pk_log(LOG_ERROR, "Couldn't begin transaction "
					"on behalf of %s()", caller);
		return PK_IOERR;
	}
	return PK_SUCCESS;
}

pk_err_t _commit(sqlite3 *db, const char *caller)
{
	if (query(NULL, db, "COMMIT", NULL)) {
		pk_log(LOG_ERROR, "Couldn't commit transaction "
					"on behalf of %s()", caller);
		return PK_IOERR;
	}
	return PK_SUCCESS;
}

pk_err_t _rollback(sqlite3 *db, const char *caller)
{
	if (query(NULL, db, "ROLLBACK", NULL)) {
		pk_log(LOG_ERROR, "Couldn't roll back transaction "
					"on behalf of %s()", caller);
		return PK_IOERR;
	}
	return PK_SUCCESS;
}

static int busy_handler(void *db, int count)
{
	int ms;

	(void)db;  /* silence warning */
	if (count == 0)
		ms=1;
	else if (count <= 2)
		ms=2;
	else if (count <= 5)
		ms=5;
	else
		ms=10;
	usleep(ms * 1000);
	return 1;
}

pk_err_t set_busy_handler(sqlite3 *db)
{
	if (sqlite3_busy_handler(db, busy_handler, db)) {
		pk_log(LOG_ERROR, "Couldn't set busy handler for database");
		return PK_CALLFAIL;
	}
	return PK_SUCCESS;
}

/* This validates both the primary and attached databases */
pk_err_t validate_db(sqlite3 *db)
{
	sqlite3_stmt *stmt;
	const char *str;
	int result;

	if (query(&stmt, db, "PRAGMA integrity_check(1)", NULL) != SQLITE_ROW) {
		query_free(stmt);
		pk_log(LOG_ERROR, "Couldn't run SQLite integrity check");
		return PK_IOERR;
	}
	query_row(stmt, "s", &str);
	result=strcmp(str, "ok");
	query_free(stmt);
	if (result) {
		pk_log(LOG_ERROR, "SQLite integrity check failed");
		return PK_BADFORMAT;
	}
	return PK_SUCCESS;
}
