/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* jksql.h - Stuff to manage interface with SQL database. 
 *
 * To use - first open a connection, then pass a SQL query to 
 * sqlGetResult, then use sqlNextRow to examine result row by
 * row. The returned row is just an array of strings.  Use
 * sqlUnsigned, sqlSigned, and atof to convert numeric results
 * to normal form. 
 *
 * These routines will all print an error message and abort if 
 * there's a problem, cleaning up open connections, etc. on abort 
 * (or on program exit).  Do a pushAbortHandler if you want to 
 * catch the aborts.  The error messages from bad SQL syntax
 * are actually pretty good (they're just passed on from
 * mySQL). */

#ifndef SQLNUM_H
#include "sqlNum.h"
#endif

#ifndef SQLLIST_H
#include "sqlList.h"
#endif

#ifndef HASH_H
#include "hash.h"
#endif
	
struct sqlConnection *sqlConnect(char *database);
/* Connect to database on default host as default user. */

struct sqlConnection *sqlConnectRemote(char *host, 
	char *user, char *password, char *database);
/* Connect to database somewhere as somebody. */

struct hash *sqlHashOfDatabases();
/* Get hash table with names of all databases that are online. */

void sqlDisconnect(struct sqlConnection **pSc);
/* Close down connection. */

struct sqlConnCache *sqlNewConnCache(char *database);
/* Return a new connection cache. (Useful if going to be
 * doing lots of different queries in different routines
 * to same database - reduces connection overhead.) */

struct sqlConnCache *sqlNewRemoteConnCache(char *database, 
	char *host, char *user, char *password);
/* Set up a cache on a remote database. */

void sqlFreeConnCache(struct sqlConnCache **pCache);
/* Dispose of a connection cache. */

struct sqlConnection *sqlAllocConnection(struct sqlConnCache *cache);
/* Allocate a cached connection. */

void sqlFreeConnection(struct sqlConnCache *cache,struct sqlConnection **pConn);
/* Free up a cached connection. */

void sqlUpdate(struct sqlConnection *conn, char *query);
/* Tell database to do something that produces no results table. */

int sqlUpdateRows(struct sqlConnection *conn, char *query, int* matched);
/* Execute an update query, returning the number of rows change.  If matched
 * is not NULL, it gets the total number matching the query. */

boolean sqlExists(struct sqlConnection *conn, char *query);
/* Query database and return TRUE if it had a non-empty result. */

struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query);
/* Query database.
 * Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query);
/* Query database. If result empty squawk and die. */

void sqlFreeResult(struct sqlResult **pRes);
/* Free up a result. */

int sqlCountColumns(struct sqlResult *sr);
/* Count the number of columns in result. */

int sqlCountRows(struct sqlConnection *sc, char *table);
/* Count the number of rows in a table. */

boolean sqlTableExists(struct sqlConnection *sc, char *table);
/* Return TRUE if a table exists. */

char *sqlQuickQuery(struct sqlConnection *sc, char *query, char *buf, int bufSize);
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Returns NULL if query comes up empty. */

int sqlQuickNum(struct sqlConnection *conn, char *query);
/* Get numerical result from simple query */

void sqlDropTable(struct sqlConnection *sc, char *table);
/* Drop table if it exists. */

boolean sqlMaybeMakeTable(struct sqlConnection *sc, char *table, char *query);
/* Create table from query if it doesn't exist already. 
 * Returns FALSE if didn't make table. */

boolean sqlRemakeTable(struct sqlConnection *sc, char *table, char *create);
/* Drop table if it exists, and recreate it. */

char **sqlNextRow(struct sqlResult *sr);
/* Fetch next row from result.  If you need to save these strings
 * past the next call to sqlNextRow you must copy them elsewhere.
 * It is ok to write to the strings - replacing tabs with zeroes
 * for instance.  You can call this with a NULL sqlResult.  It
 * will then return a NULL row. */

char* sqlFieldName(struct sqlResult *sr);
/* repeated calls to this function returns the names of the fields 
 * the given result */

int sqlTableSize(struct sqlConnection *conn, char *table);
/* Find number of rows in table. */

int sqlFieldIndex(struct sqlConnection *conn, char *table, char *field);
/* Returns index of field in a row from table, or -1 if it 
 * doesn't exist. */

unsigned int sqlLastAutoId(struct sqlConnection *conn);
/* Return last automatically incremented id inserted into database. */

void sqlVaWarn(struct sqlConnection *sc, char *format, va_list args);
/* Error message handler. */

void sqlWarn(struct sqlConnection *sc, char *format, ...);
/* Printf formatted error message that adds on sql 
 * error message. */

void sqlAbort(struct sqlConnection  *sc, char *format, ...);
/* Printf formatted error message that adds on sql 
 * error message and abort. */

void sqlCleanupAll();
/* Cleanup all open connections and resources. */

