/*
 *  The OpenDiamond Platform for Interactive Search
 *  Version 3
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
 * SQLite3 helper functions written by Benjamin Gilbert for
 *
 * Parcelkeeper - support daemon for the OpenISR (TM) system virtual disk
 *
 * Copyright (C) 2006-2007 Carnegie Mellon University
 */

#ifndef __SQL_H__
#define __SQL_H__

#include <sqlite3.h>

int sql_query(sqlite3_stmt **result, sqlite3 *db, const char *query,
	      const char *fmt, ...);
int sql_query_next(sqlite3_stmt *stmt);
void sql_query_row(sqlite3_stmt *stmt, const char *fmt, ...);
void sql_query_free(sqlite3_stmt *stmt);

int sql_attach(sqlite3 *db, const char *handle, const char *file);
int sql_set_busy_handler(sqlite3 *db);
int sql_validate_db(sqlite3 *db);

#define sql_begin(db)    _sql_begin(__func__, db)
#define sql_commit(db)   _sql_commit(__func__, db)
#define sql_rollback(db) _sql_rollback(__func__, db)

int _sql_begin(const char *caller, sqlite3 *db);
int _sql_commit(const char *caller, sqlite3 *db);
int _sql_rollback(const char *caller, sqlite3 *db);

#endif /* __SQL_H__ */

