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

#include "sqlNum.h"
#include "sqlList.h"
#include "hash.h"
#include "dystring.h"
#include "asParse.h"

char *getDefaultProfileName();  // name of default profile

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
 * just connect to the server. Abort on error. 
 * This only takes limited connection parameters. Use Full version for access to all.*/

struct sqlConnection *sqlMayConnectRemote(char *host, char *user, char *password,
                                          char *database);
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server.  Return NULL if can't connect. 
 * This only takes limited connection parameters. Use Full version for access to all.*/

struct sqlConnection *sqlConnectRemoteFull(struct slPair *pairs, char *database);
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server. Abort on error. 
 * Connection parameter pairs contains a list of name/values. */

struct sqlConnection *sqlMayConnectRemoteFull(struct slPair *pairs, char *database);
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server.  
 * Connection parameter pairs contains a list of name/values. Return NULL if can't connect.*/

void sqlProfileConfig(struct slPair *pairs);
/* Set configuration for the profile.  This overrides an existing profile in
 * hg.conf or defines a new one.  Results are unpredictable if a connect cache
 * has been established for this profile. */

void sqlProfileConfigDefault(struct slPair *pairs);
/* Set configuration for the default profile.  This overrides an existing
 * profile in hg.conf or defines a new one.  Results are unpredictable if a
 * connect cache has been established for this profile. */

char *sqlProfileToMyCnf(char *profileName);
/* Read in profile named, 
 * and create a multi-line setting string usable in my.cnf files.  
 * Return Null if profile not found. */

void sqlProfileAddDb(char *profileName, char *db);
/* add a mapping of db to profile.  If database is already associated with
 * this profile, it is ignored.  If it is associated with a different profile,
 * it is an error. */

struct slName* sqlProfileGetNames();
/* Get a list of all profile names. slFreeList result when done */

struct hash *sqlHashOfDatabases(void);
/* Get hash table with names of all databases that are online. */

struct slName *sqlListOfDatabases(void);
/* Get list of all databases that are online. */

char *sqlHostInfo(struct sqlConnection *sc);
/* Returns the mysql host info for the connection, must be connected. */

void sqlDisconnect(struct sqlConnection **pSc);
/* Close down connection. */

char* sqlGetDatabase(struct sqlConnection *sc);
/* Get the database associated with an connection. Warning: return may be NULL! */

char* sqlGetHost(struct sqlConnection *sc);
/* Get the host associated with an connection. */

struct slName *sqlGetAllDatabase(struct sqlConnection *sc);
/* Get a list of all database on the server */

struct slName *sqlListTablesLike(struct sqlConnection *conn, char *likeExpr);
/* Return list of tables in database associated with conn. Optionally filter list with
 * given LIKE expression that can be NULL or string e.g. "LIKE 'snp%'". */

struct slName *sqlListTables(struct sqlConnection *conn);
/* Return list of tables in database associated with conn. */

struct slName *sqlListFields(struct sqlConnection *conn, char *table);
/* Return list of fields in table. */

struct sqlResult *sqlDescribe(struct sqlConnection *conn, char *table);
/* Run the sql DESCRIBE command or get a cached table description and return the sql result */

void sqlAddDatabaseFields(char *database, struct hash *hash);
/* Add fields from the one database to hash. */

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

struct sqlConnection *sqlConnCacheProfileAllocMaybe(struct sqlConnCache *cache,
                                                    char *profileName,
                                                    char *database);
/* Allocate a cached connection given a profile and/or database. Return NULL
 * if the database doesn't exist.  */

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

int sqlRowCount(struct sqlConnection *conn, char *queryTblAndCondition);
/* Return count of rows that match condition. The queryTblAndCondition
 * should contain everying after "select count(*) FROM " */

/* Options to sqlLoadTabFile */

#define SQL_TAB_FILE_ON_SERVER 0x01  /* tab file is directly accessible
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
 * Options are the SQL_TAB_* bit set. SQL_TAB_FILE_ON_SERVER is ignored if
 * sqlIsRemote() returns true. */

struct sqlResult *sqlGetResultExt(struct sqlConnection *sc, char *query, unsigned int *errorNo, char **error);
/* Returns NULL if it had an error.
 * Otherwise returns a structure that you can do sqlRow() on.
 * If there was an error, *errorNo will be set to the mysql error number,
 * and *error will be set to the mysql error string, which MUST NOT be freed. */

struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query);
/* (Returns NULL if result was empty. :
 *     old info, only applies with mysql_store_result not mysql_use_result)
 * Otherwise returns a structure that you can do sqlRow() on. */

unsigned long sqlEscapeStringFull(char *to, const char* from, long fromLength);
/* Prepares a string for inclusion in a sql statement.  Output string
 * must be 2*strlen(from)+1. fromLength is the length of the from data.
 * Specifying fromLength allows one to encode a binary string that can contain any character including 0. */

char *sqlEscapeString(const char* from);
/* Prepares string for inclusion in a SQL statement . Remember to free
 * returned string.  Returned string contains strlen(length)*2+1 as many bytes
 * as orig because in worst case every character has to be escaped.*/

char *sqlEscapeString2(char *to, const char* from);
/* Prepares a string for inclusion in a sql statement.  Output string
 * must be 2*strlen(from)+1 */

unsigned long sqlEscapeString3(char *to, const char* from);
/* Prepares a string for inclusion in a sql statement.  Output string
 * must be 2*strlen(from)+1.  Returns actual escaped size not counting term 0. */

void sqlDyAppendEscaped(struct dyString *dy, char *s);
/* Append to dy an escaped s */

char *sqlEscapeTabFileString2(char *to, const char *from);
/* Escape a string for including in a tab seperated file. Output string
 * must be 2*strlen(from)+1 */

char *sqlEscapeTabFileString(const char *from);
/* Escape a string for including in a tab seperated file. Freez or freeMem
 * result when done. */

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query);
/* Query database.
 * old comment: If result empty squawk and die.
 *    This only applied back when sqlGetResult was using mysql_store_result.
 * These days, with mysql_use_result, we cannot know ahead of time
 * if there are results, we can only know by actually trying to fetch a row.
 * At then how would we put it back?  So in fact right now sqlMustGetResult
 * is no different than sqlGetResult.  */

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

bool sqlColumnExists(struct sqlConnection *conn, char *tableName, char *column);
/* return TRUE if column exists in table. column can contain sql wildcards  */

int sqlTableSizeIfExists(struct sqlConnection *sc, char *table);
/* Return row count if a table exists, -1 if it doesn't. */

boolean sqlTablesExist(struct sqlConnection *conn, char *tables);
/* Check all tables in space delimited string exist. */

boolean sqlTableWildExists(struct sqlConnection *sc, char *table);
/* Return TRUE if table (which can include SQL wildcards) exists.
 * A bit slower than sqlTableExists. */

unsigned long sqlTableDataSizeFromSchema(struct sqlConnection *conn, char *db, char *table);
/* Get table data size. Table must exist or will abort. */

unsigned long sqlTableIndexSizeFromSchema(struct sqlConnection *conn, char *db, char *table);
/* Get table index size. Table must exist or will abort. */

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

long long sqlQuickLongLong(struct sqlConnection *conn, char *query);
/* Get long long numerical result from simple query. Returns 0 if query not found */

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

struct slPair *sqlQuickPairList(struct sqlConnection *conn, char *query);
/* Return a list of slPairs with the results of a two-column query.
 * Free result with slPairFreeValsAndList. */

void sqlRenameTable(struct sqlConnection *sc, char *table1, char *table2);
/* Rename table */

void sqlCopyTable(struct sqlConnection *sc, char *table1, char *table2);
/* Copy table1 to table2 */

void sqlDropTable(struct sqlConnection *sc, char *table);
/* Drop table if it exists. */

void sqlGetLockWithTimeout(struct sqlConnection *sc, char *name, int wait);
/* Tries to get an advisory lock on the process, waiting for wait seconds. */
/* Blocks another client from obtaining a lock with the same name. */

void sqlGetLock(struct sqlConnection *sc, char *name);
/* Gets an advisory lock created by GET_LOCK in sqlGetLock. Waits up to 1000 seconds. */

boolean sqlIsLocked(struct sqlConnection *sc, char *name);
/* Tests if an advisory lock on the given name has been set. 
 * Returns true if lock has been set, otherwise returns false. */

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

char *sqlGetCreateTable(struct sqlConnection *sc, char *table);
/* Get the Create table statement. table must exist. */

void sqlRemakeTable(struct sqlConnection *sc, char *table, char *create);
/* Drop table if it exists, and recreate it. */

char **sqlNextRow(struct sqlResult *sr);
/* Fetch next row from result.  If you need to save these strings
 * past the next call to sqlNextRow you must copy them elsewhere.
 * It is ok to write to the strings - replacing tabs with zeroes
 * for instance.  You can call this with a NULL sqlResult.  It
 * will then return a NULL row. */

char* sqlFieldName(struct sqlResult *sr);
/* Repeated calls to this function returns the names of the fields
 * the given result. */

struct slName *sqlResultFieldList(struct sqlResult *sr);
/* Return slName list of all fields in query.  Can just be done once per query. */

int sqlResultFieldArray(struct sqlResult *sr, char ***retArray);
/* Get the fields of sqlResult,  returning count, and the results
 * themselves in *retArray. */

int sqlFieldColumn(struct sqlResult *sr, char *colName);
/* get the column number of the specified field in the result, or
 * -1 if the result doesn't contain the field.*/

int sqlTableSize(struct sqlConnection *conn, char *table);
/* Find number of rows in table. */

int sqlFieldIndex(struct sqlConnection *conn, char *table, char *field);
/* Returns index of field in a row from table, or -1 if it
 * doesn't exist. */

struct slName *sqlFieldNames(struct sqlConnection *conn, char *table);
/* Returns field names from a table. */

unsigned int sqlLastAutoId(struct sqlConnection *conn);
/* Return last automatically incremented id inserted into database. */

void sqlVaWarn(struct sqlConnection *sc, char *format, va_list args);
/* Error message handler. */

void sqlWarn(struct sqlConnection *sc, char *format, ...)
/* Printf formatted error message that adds on sql
 * error message. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

void sqlAbort(struct sqlConnection  *sc, char *format, ...)
/* Printf formatted error message that adds on sql
 * error message and abort. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

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

long sqlDateToUnixTime(char *sqlDate);
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

time_t sqlTableUpdateTime(struct sqlConnection *conn, char *table);
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

bool sqlCanCreateTemp(struct sqlConnection *conn);
/* Return True if it looks like we can write into temp tables in the current database
 * Can be used to check if sqlRandomSampleWithSeed-functions are safe to call.
 * */

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

void sqlSetParanoid(boolean beParanoid);
/* If set to TRUE, will make more diagnostic stderr messages. */

boolean sqlIsRemote(struct sqlConnection *conn);
/* test if the conn appears to be to a remote system.
 * Current only tests for a TCP/IP connection */

void sqlWarnings(struct sqlConnection *conn, int numberOfWarnings);
/* Show the number of warnings requested. New feature in mysql5. */

void sqlDump(FILE *fh);
/* dump internal info about SQL configuration for debugging purposes */

void sqlPrintStats(FILE *fh);
/* print statistic about the number of connections and other options done by
 * this process. */

struct sqlResult *sqlStoreResult(struct sqlConnection *sc, char *query);
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on.  Same interface as sqlGetResult,
 * but internally this keeps the entire result in memory. */



/* --------- input checks to prevent sql injection --------------------------------------- */

#define sqlCkIl sqlCheckIdentifiersList
char *sqlCheckIdentifiersList(char *identifiers);
/* Check that only valid identifier characters are used in a comma-separated list */

#define sqlCkId sqlCheckIdentifier
char *sqlCheckIdentifier(char *identifier);
/* Check that only valid identifier characters are used */


// =============================

int vaSqlSafefNoAbort(char* buffer, int bufSize, boolean newString, char *format, va_list args);
/* VarArgs Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte.
 * Scans string parameters for illegal sql chars. 
 * Automatically escapes quoted string values.
 * This function should be efficient on statements with many strings to be escaped. */

int vaSqlSafef(char* buffer, int bufSize, char *format, va_list args);
/* VarArgs Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte. 
 * Scans unquoted string parameters for illegal literal sql chars.
 * Escapes quoted string parameters. 
 * NOSLQINJ tag is added to beginning. */

int sqlSafef(char* buffer, int bufSize, char *format, ...)
/* Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte. 
 * Scans unquoted string parameters for illegal literal sql chars.
 * Escapes quoted string parameters. 
 * NOSLQINJ tag is added to beginning. */
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;


int vaSqlSafefFrag(char* buffer, int bufSize, char *format, va_list args);
/* VarArgs Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte.
 * Scans unquoted string parameters for illegal literal sql chars.
 * Escapes quoted string parameters. 
 * NOSLQINJ tag is NOT added to beginning since it is assumed to be just a fragment of
 * the entire sql string. */

int sqlSafefFrag(char* buffer, int bufSize, char *format, ...)
/* Format string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte.
 * Scans unquoted string parameters for illegal literal sql chars.
 * Escapes quoted string parameters. 
 * NOSLQINJ tag is NOT added to beginning since it is assumed to be just a fragment of
 * the entire sql string. */
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;


int sqlSafefAppend(char* buffer, int bufSize, char *format, ...)
/* Append formatted string to buffer, vsprintf style, only with buffer overflow
 * checking.  The resulting string is always terminated with zero byte.
 * Scans unquoted string parameters for illegal literal sql chars.
 * Escapes quoted string parameters. 
 * NOSLQINJ tag is NOT added to beginning since it is assumed to be appended to
 * a properly created sql string. */
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;


void vaSqlDyStringPrintfExt(struct dyString *ds, boolean isFrag, char *format, va_list args);
/* VarArgs Printf to end of dyString after scanning string parameters for illegal sql chars.
 * Strings inside quotes are automatically escaped.  
 * NOSLQINJ tag is added to beginning if it is a new empty string and isFrag is FALSE.
 * Appends to existing string. */

void vaSqlDyStringPrintf(struct dyString *ds, char *format, va_list args);
/* Printf to end of dyString after scanning string parameters for illegal sql chars.
 * Strings inside quotes are automatically escaped.  
 * NOSLQINJ tag is added to beginning if it is a new empty string.
 * Appends to existing string. */

void sqlDyStringPrintf(struct dyString *ds, char *format, ...)
/* Printf to end of dyString after scanning string parameters for illegal sql chars.
 * Strings inside quotes are automatically escaped.  
 * NOSLQINJ tag is added to beginning if it is a new empty string.
 * Appends to existing string. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

void vaSqlDyStringPrintfFrag(struct dyString *ds, char *format, va_list args);
/* VarArgs Printf to end of dyString after scanning string parameters for illegal sql chars.
 * Strings inside quotes are automatically escaped.
 * NOSLQINJ tag is NOT added to beginning since it is assumed to be just a fragment of
 * the entire sql string. Appends to existing string. */

void sqlDyStringPrintfFrag(struct dyString *ds, char *format, ...)
/* Printf to end of dyString after scanning string parameters for illegal sql chars.
 * Strings inside quotes are automatically escaped.
 * NOSLQINJ tag is NOT added to beginning since it is assumed to be just a fragment of
 * the entire sql string. Appends to existing string. */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
;

#define NOSQLINJ "NOSQLINJ "

struct dyString *sqlDyStringCreate(char *format, ...)
/* Create a dyString with a printf style initial content
 * Adds the NOSQLINJ prefix. */
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

void sqlDyStringPrintIdList(struct dyString *ds, char *fields);
/* Append a comma-separated list of field identifiers. Aborts if invalid characters in list. */

void sqlDyStringPrintValuesList(struct dyString *ds, struct slName *values);
/* Append a comma-separated, quoted and escaped list of values. */

void sqlCheckError(char *format, ...)
/* A sql injection error has occurred. Check for settings and respond
 * as appropriate with error, warning, logOnly, ignore, dumpstack.
 * Then abort if needed. NOTE: unless it aborts, this function will return! */
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;

struct sqlConnection *sqlFailoverConn(struct sqlConnection *sc);
/* returns the failover connection of a connection or NULL.
 * (Needed because the sqlConnection is not in the .h file) */

/* structure moved here from hgTables.h 2019-03-04 - Hiram */
struct sqlFieldType
/* List field names and types */
    {
    struct sqlFieldType *next;
    char *name;         /* Name of field. */
    char *type;         /* Type of field (MySQL notion) */
    };

struct sqlFieldType *sqlFieldTypeNew(char *name, char *type);
/* Create a new sqlFieldType */

void sqlFieldTypeFree(struct sqlFieldType **pFt);
/* Free resources used by sqlFieldType */

void sqlFieldTypeFreeList(struct sqlFieldType **pList);
/* Free a list of dynamically allocated sqlFieldType's */

struct sqlFieldType *sqlFieldTypesFromAs(struct asObject *as);
/* Convert asObject to list of sqlFieldTypes */

struct sqlFieldType *sqlListFieldsAndTypes(struct sqlConnection *conn, char *table);
/* Get list of fields including their names and types.  The type currently is
 * just a MySQL type string. */



#endif /* JKSQL_H */
