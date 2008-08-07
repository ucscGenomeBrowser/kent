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
#ifndef JKSQL_H
#define JKSQL_H
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

struct sqlConnection *sqlMayConnect(char *database);
/* Connect to database on default host as default user. 
 * Return NULL (don't abort) on failure. */

struct sqlConnection *sqlConnectProfile(char *profileName, char *database);
/* Connect to profile or database using the specified profile.  Can specify
 * profileName, database, or both. The profile is the prefix to the host,
 * user, and password variables in .hg.conf.  For the default profile of "db",
 * the environment variables HGDB_HOST, HGDB_USER, and HGDB_PASSWORD can
 * override.
 */ 

struct sqlConnection *sqlMayConnectProfile(char *profileName, char *database);
/* Connect to profile or database using the specified profile. Can specify
 * profileName, database, or both. The profile is the prefix to the host,
 * user, and password variables in .hg.conf.  For the default profile of "db",
 * the environment variables HGDB_HOST, HGDB_USER, and HGDB_PASSWORD can
 * override.  Return NULL if connection fails.
 */ 

struct sqlConnection *sqlConnectRemote(char *host, char *user, char *password,
                                       char *database);
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server. Abort on error. */

struct sqlConnection *sqlMayConnectRemote(char *host, char *user, char *password,
                                          char *database);
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server.  Return NULL can't connect */

struct hash *sqlHashOfDatabases(void);
/* Get hash table with names of all databases that are online. */

struct slName *sqlListOfDatabases(void);
/* Get list of all databases that are online. */

void sqlDisconnect(struct sqlConnection **pSc);
/* Close down connection. */

char* sqlGetDatabase(struct sqlConnection *sc);
/* Get the database associated with an connection. */

struct slName *sqlGetAllDatabase(struct sqlConnection *sc);
/* Get a list of all database on the server */

struct slName *sqlListTables(struct sqlConnection *conn);
/* Return list of tables in database associated with conn. */

struct slName *sqlListFields(struct sqlConnection *conn, char *table);
/* Return list of fields in table. */

struct hash *sqlAllFields(void);
/* Get hash of all fields in database.table.field format.  */

struct sqlConnCache *sqlConnCacheNew();
/* Return a new connection cache. (Useful if going to be
 * doing lots of different queries in different routines
 * to same database - reduces connection overhead.) */

struct sqlConnCache *sqlConnCacheNewRemote(char *host, char *user,
                                           char *password);
/* Set up a cache on a remote database. */

struct sqlConnCache *sqlConnCacheNewProfile(char *profileName);
/* Return a new connection cache associated with the particular profile. */

void sqlConnCacheFree(struct sqlConnCache **pCache);
/* Dispose of a connection cache. */

struct sqlConnection *sqlConnCacheMayAlloc(struct sqlConnCache *cache,
                                           char *database);
/* Allocate a cached connection. errAbort if too many open connections,
 * return NULL if can't connect to server. */

struct sqlConnection *sqlConnCacheAlloc(struct sqlConnCache *cache,
                                        char *database);
/* Allocate a cached connection. */

struct sqlConnection *sqlConnCacheProfileAlloc(struct sqlConnCache *cache,
                                               char *profileName,
                                               char *database);
/* Allocate a cached connection given a profile and/or database. */

void sqlConnCacheDealloc(struct sqlConnCache *cache,struct sqlConnection **pConn);
/* Free up a cached connection. */

void sqlUpdate(struct sqlConnection *conn, char *query);
/* Tell database to do something that produces no results table. */

int sqlUpdateRows(struct sqlConnection *conn, char *query, int* matched);
/* Execute an update query, returning the number of rows change.  If matched
 * is not NULL, it gets the total number matching the query. */

boolean sqlExists(struct sqlConnection *conn, char *query);
/* Query database and return TRUE if it had a non-empty result. */


boolean sqlRowExists(struct sqlConnection *conn,
	char *table, char *field, char *key);
/* Return TRUE if row where field = key is in table. */


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
#define SQL_TAB_REPLACE            0x40  /* Replace entries with duplicate
                                          * unique keys instead of generating an
                                          * error. */

void sqlLoadTabFile(struct sqlConnection *conn, char *path, char *table,
                    unsigned options);
/* Load a tab-seperated file into a database table, checking for errors. 
 * Options are the SQL_TAB_* bit set. */

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

int sqlCountColumnsInTable(struct sqlConnection *sc, char *table);
/* Return the number of columns in a table */

boolean sqlDatabaseExists(char *database);
/* Return TRUE if database exists. */

boolean sqlTableExists(struct sqlConnection *sc, char *table);
/* Return TRUE if a table exists. */

int sqlTableSizeIfExists(struct sqlConnection *sc, char *table);
/* Return row count if a table exists, -1 if it doesn't. */

boolean sqlTablesExist(struct sqlConnection *conn, char *tables);
/* Check all tables in space delimited string exist. */

boolean sqlTableWildExists(struct sqlConnection *sc, char *table);
/* Return TRUE if table (which can include SQL wildcards) exists. 
 * A bit slower than sqlTableExists. */

boolean sqlTableOk(struct sqlConnection *sc, char *table);
/* Return TRUE if a table not only exists, but also is not corrupted. */

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

double sqlQuickDouble(struct sqlConnection *conn, char *query);
/* Get floating point numerical result from simple query */

char *sqlQuickString(struct sqlConnection *conn, char *query);
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd. */

char *sqlNeedQuickString(struct sqlConnection *sc, char *query);
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd.  This will
 * print an error message and abort if result returns empty. */

char *sqlQuickNonemptyString(struct sqlConnection *conn, char *query);
/* Return first result of given query.  If it is an empty string
 * convert it to NULL. */

struct slName *sqlQuickList(struct sqlConnection *conn, char *query);
/* Return a list of slNames for a single column query.
 * Do a slFreeList on result when done. */

struct hash *sqlQuickHash(struct sqlConnection *conn, char *query);
/* Return a hash filled with results of two column query. 
 * The first column is the key, the second the value. */

struct slInt *sqlQuickNumList(struct sqlConnection *conn, char *query);
/* Return a list of slInts for a single column query.
 * Do a slFreeList on result when done. */

struct slDouble *sqlQuickDoubleList(struct sqlConnection *conn, char *query);
/* Return a list of slDoubles for a single column query.
 * Do a slFreeList on result when done. */

void sqlDropTable(struct sqlConnection *sc, char *table);
/* Drop table if it exists. */

void sqlGetLock(struct sqlConnection *sc, char *name);
/* Sets an advisory lock on the process for 1000s returns 1 if successful,*/
/* 0 if name already locked or NULL if error occurred */
/* blocks another client from obtaining a lock with the same name */

void sqlReleaseLock(struct sqlConnection *sc, char *name);
/* Releases an advisory lock created by GET_LOCK in sqlGetLock */

void sqlHardLockTables(struct sqlConnection *sc, struct slName *tableList, 
	boolean isWrite);
/* Hard lock given table list.  Unlock with sqlHardUnlockAll. */

void sqlHardLockTable(struct sqlConnection *sc, char *table, boolean isWrite);
/* Lock a single table.  Unlock with sqlHardUnlockAll. */

void sqlHardLockAll(struct sqlConnection *sc, boolean isWrite);
/* Lock all tables in current database.  Unlock with sqlHardUnlockAll. */

void sqlHardUnlockAll(struct sqlConnection *sc);
/* Unlock any hard locked tables. */

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

int sqlFieldColumn(struct sqlResult *sr, char *colName);
/* get the column number of the specified field in the result, or
 * -1 if the result doesn't contailed the field.*/

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

void sqlCleanupAll(void);
/* Cleanup all open connections and resources. */

boolean sqlWildcardIn(char *s);
/* Return TRUE if there is a sql wildcard char in string. */

char *sqlLikeFromWild(char *wild);
/* Convert normal wildcard string to SQL wildcard by
 * mapping * to % and ? to _.  Escape any existing % and _'s. */

/* flags controlling mysql tracing and time logging */
#define JKSQL_TRACE   0x01   /* enable tracing of each mysql query to stderr */
#define JKSQL_PROF    0x02   /* record time spend in database queries,
                              * and dump at exit. */

void sqlMonitorEnable(unsigned flags);
/* Enable disable tracing or profiling of SQL queries.
 * If JKSQL_TRACE is specified, then tracing of each SQL query is enabled,
 * along with the timing of the queries.
 * If JKSQL_PROF is specified, then time spent in SQL queries is logged
 * and printed when the program exits or when sqlMonitorDisable is called.
 *
 * These options can also be enabled by setting the JKSQL_TRACE and/or
 * JKSQL_PROF environment variables to "on".  The cheapcgi module will set
 * these environment variables if the corresponding CGI variables are set
 * to "on".  These may also be set in the .hg.conf file.  While this method
 * of setting these parameters is a bit of a hack, it avoids uncessary
 * dependencies.
 */

void sqlMonitorSetIndent(unsigned indent);
/* set the sql indent level indent to the number of spaces to indent each
 * trace, which can be helpful in making voluminous trace info almost
 * readable. */

void sqlMonitorDisable(void);
/* Disable tracing or profiling of SQL queries. */

int sqlDateToUnixTime(char *sqlDate);
/* Convert a SQL date such as "2003-12-09 11:18:43" to clock time 
 * (seconds since midnight 1/1/1970 in UNIX). */

char *sqlUnixTimeToDate(time_t *timep, boolean gmTime);
/* Convert a clock time (seconds since 1970-01-01 00:00:00 unix epoch)
 *	to the string: "YYYY-MM-DD HH:MM:SS"
 *  returned string is malloced, can be freed after use
 *  boolean gmTime requests GMT time instead of local time
 */

char *sqlTableUpdate(struct sqlConnection *conn, char *table);
/* Get last update time for table as an SQL string */

int sqlTableUpdateTime(struct sqlConnection *conn, char *table);
/* Get last update time for table (in Unix terms). */

char *sqlGetPrimaryKey(struct sqlConnection *conn, char *table);
/* Get primary key if any for table, return NULL if none. */

char** sqlGetEnumDef(struct sqlConnection *conn, char* table, char* colName);
/* Get the definitions of a enum column in a table, returning a
 * null-terminated array of enum values.  Free array when finished.  */

struct slName *sqlRandomSample(char *db, char *table, char *field, int count);
/* Get random sample from database. */

struct slName *sqlRandomSampleConn(struct sqlConnection *conn, char *table,
				   char *field, int count);
/* Get random sample from conn. */

struct slName *sqlRandomSampleWithSeed(char *db, char *table, char *field, int count, int seed);
/* Get random sample from database specifiying rand number seed, or -1 for none */


struct sqlFieldInfo
/* information about fields of a table; free with sqlFieldInfoFreeList */
{
    struct sqlFieldInfo *next;
    char *field;          /* name of field */
    char *type;           /* type of field */
    boolean allowsNull;   /* can the field be NULL? */
    char *key;            /* key information */
    char *defaultVal;     /* default value */
    char *extra;          /* extra info */
};

struct sqlFieldInfo *sqlFieldInfoGet(struct sqlConnection *conn, char *table);
/* get a list of objects describing the fields of a table */

void sqlFieldInfoFreeList(struct sqlFieldInfo **fiListPtr);
/* Free a list of sqlFieldInfo objects */

enum sqlQueryOpts
/* options controls behavior of sqlQueryObj */
{
    sqlQueryMust   = 0x01, /* must get a row back, or error */
    sqlQuerySingle = 0x02, /* must get no more than one row back */
    sqlQueryMulti  = 0x04  /* can get more than one row back */
};

/* type of function for loading objects from rows. */
typedef struct slList *(*sqlLoadFunc)(char **row);

void *sqlVaQueryObjs(struct sqlConnection *conn, sqlLoadFunc loadFunc,
                     unsigned opts, char *queryFmt, va_list args);
/* Generate a query from format and args.  Load one or more objects from rows
 * using loadFunc.  Check the number of rows returned against the sqlQueryOpts
 * bit set.  Designed for use with autoSql, although load function must be
 * cast to sqlLoadFunc. */

void *sqlQueryObjs(struct sqlConnection *conn, sqlLoadFunc loadFunc,
                   unsigned opts, char *queryFmt, ...)
/* Generate a query from format and args.  Load one or more objects from rows
 * using loadFunc.  Check the number of rows returned against the sqlQueryOpts
 * bit set.  Designed for use with autoSql, although load function must be
 * cast to sqlLoadFunc. */
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
;

int sqlSaveQuery(struct sqlConnection *conn, char *query, char *outPath, boolean isFa);
/* Execute query, save the resultset as a tab-separated file.
 * If isFa is true, than assume it is a two column fasta query and format accordingly.
 * Return count of rows in result set.  Abort on error. */

char *sqlVersion(struct sqlConnection *conn);
/* Return version of MySQL database.  This will be something
 * of the form 5.0.18-standard. */

int sqlMajorVersion(struct sqlConnection *conn);
/* Return major version of database. */

int sqlMinorVersion(struct sqlConnection *conn);
/* Return minor version of database. */

char *sqlTempTableName(struct sqlConnection *conn, char *prefix);
/* Return a name for a temporary table. Name will start with
 * prefix.  This call doesn't actually  make table.  (So you should 
 * make table before next call to insure uniqueness.)  However the
 * table name encorperates the host, pid, and time, which helps insure
 * uniqueness between different processes at least.  FreeMem the result
 * when you are done. */

#endif /* JKSQL_H */
