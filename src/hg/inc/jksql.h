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

extern boolean sqlTrace;      /* setting to true prints each query */
extern int sqlTraceIndent;    /* number of spaces to indent traces */
	
struct sqlConnection *sqlConnect(char *database);
/* Connect to database on default host as default user. */

struct sqlConnection *sqlMayConnect(char *database);
/* Connect to database on default host as default user. 
 * Return NULL (don't abort) on failure. */

struct sqlConnection *sqlConnectReadOnly(char *database);
/* Connect to database using ro profile in .hg.conf */ 

struct sqlConnection *sqlConnectRemote(char *host, 
	char *user, char *password, char *database);
/* Connect to database somewhere as somebody. */

struct hash *sqlHashOfDatabases();
/* Get hash table with names of all databases that are online. */

void sqlDisconnect(struct sqlConnection **pSc);
/* Close down connection. */

char* sqlGetDatabase(struct sqlConnection *sc);
/* Get the database associated with an connection. */

struct slName *sqlGetAllDatabase(struct sqlConnection *sc);
/* Get a list of all database on the server */

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

/* Return TRUE if row where field = key is in table. */

boolean sqlRowExists(struct sqlConnection *conn,
	char *table, char *field, char *key);
/* Options to sqlLoadTabFile */

#define SQL_TAB_FILE_ON_SERVER 0x01  /* tab file is directly accessable
                                     * by the sql server */
#define SQL_TAB_FILE_WARN_ON_WARN  0x02 /* warn on warnings being returned
                                         * rather than abort */
#define SQL_TAB_FILE_WARN_ON_ERROR 0x04 /* warn on errors and warnings being
                                           returned rather than abort */
#define SQL_TAB_FILE_CONCURRENT    0x10  /* optimize for allowing concurrent
                                          * access to the table. */
#define SQL_TAB_TRANSACTION_SAFE   0x20  /* Don't use speed optimizations that 
                                          * implicitly commit the current transaction. 
					  * For example "alter table" */

void sqlLoadTabFile(struct sqlConnection *conn, char *path, char *table,
                    unsigned options);
/* Load a tab-seperated file into a database table, checking for errors. 
 * Options are SQL_TAB_FILE_ON_SERVER, SQL_TAB_FILE_WARN_ON_WARN
 * SQL_TAB_FILE_WARN_ON_ERROR, SQL_TAB_FILE_CONCURRENT, SQL_TAB_TRANSACTION_SAFE */

struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query);
/* Query database.
 * Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */

char *sqlEscapeTabFileString2(char *to, const char *from);
/* Escape a string for including in a tab seperated file. Output string
 * must be 2*strlen(from)+1 */

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query);
/* Query database. If result empty squawk and die. */

void sqlFreeResult(struct sqlResult **pRes);
/* Free up a result. */

int sqlCountColumns(struct sqlResult *sr);
/* Count the number of columns in result. */

int sqlCountRows(struct sqlConnection *sc, char *table);
/* Count the number of rows in a table. */

boolean sqlDatabaseExists(char *database);
/* Return TRUE if database exists. */

boolean sqlTableExists(struct sqlConnection *sc, char *table);
/* Return TRUE if a table exists. */

boolean sqlTablesExist(struct sqlConnection *conn, char *tables);
/* Check all tables in space delimited string exist. */

char *sqlQuickQuery(struct sqlConnection *sc, char *query, char *buf, int bufSize);
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Returns NULL if query comes up empty. */

char *sqlNeedQuickQuery(struct sqlConnection *sc, char *query, 
	char *buf, int bufSize);
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Prints error message and aborts if query comes up empty. */

int sqlQuickNum(struct sqlConnection *conn, char *query);
/* Get numerical result from simple query. Returns 0 
 * if query returns no result. */

int sqlNeedQuickNum(struct sqlConnection *conn, char *query);
/* Get numerical result or die trying. */

char *sqlQuickString(struct sqlConnection *conn, char *query);
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd. */

char *sqlNeedQuickString(struct sqlConnection *sc, char *query);
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd.  This will
 * print an error message and abort if result returns empty. */

struct slName *sqlQuickList(struct sqlConnection *conn, char *query);
/* Return a list of slNames for a single column query.
 * Do a slFreeList on result when done. */

void sqlDropTable(struct sqlConnection *sc, char *table);
/* Drop table if it exists. */

boolean sqlMaybeMakeTable(struct sqlConnection *sc, char *table, char *query);
/* Create table from query if it doesn't exist already. 
 * Returns FALSE if didn't make table. */

void sqlRemakeTable(struct sqlConnection *sc, char *table, char *create);
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

char *connGetDatabase(struct sqlConnCache *conn);
/* return database for a connection cache */

char *sqlLikeFromWild(char *wild);
/* Convert normal wildcard string to SQL wildcard by
 * mapping * to % and ? to _.  Escape any existing % and _'s. */
