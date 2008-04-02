/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* jksql.c - Stuff to manage interface with SQL database. */
#include "common.h"
#include "portable.h"
#include "errabort.h"
#include <mysql.h>
#include "dlist.h"
#include "dystring.h"
#include "jksql.h"
#include "sqlNum.h"
#include "hgConfig.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: jksql.c,v 1.107 2008/03/19 04:39:03 angie Exp $";

/* flags controlling sql monitoring facility */
static unsigned monitorInited = FALSE;      /* initialized yet? */
static unsigned monitorFlags = 0;           /* flags indicating what is traced */
static long monitorEnterTime = 0;           /* time current tasked started */
static long long sqlTotalTime = 0;          /* total real milliseconds */
static long sqlTotalQueries = 0;            /* total number of queries */
static boolean monitorHandlerSet = FALSE;   /* is exit handler installed? */
static unsigned traceIndent = 0;            /* how much to indent */
static char * indentStr = "                                                       ";

struct sqlConnection
/* This is an item on a list of sql open connections. */
    {
    MYSQL *conn;		    /* Connection. */
    struct dlNode *node;	    /* Pointer to list node. */
    struct dlList *resultList;	    /* Any open results. */
    boolean hasHardLock;	    /* TRUE if table has a non-advisory lock. */
    };

struct sqlResult
/* This is an item on a list of sql open results. */
    {
    MYSQL_RES *result;			/* Result. */
    struct dlNode *node;		/* Pointer to list node we're on. */
    struct sqlConnection *conn;		/* Pointer to connection. */
    };

static struct dlList *sqlOpenConnections;

char* getCfgValue(char* envName, char* cfgName)
/* get a configuration value, from either the environment or the cfg file,
 * with the env take precedence.
 */
{
char *val = getenv(envName);
if (val == NULL || (0 == strlen(val)))
    val = cfgOption(cfgName);
return val;
}

static void monitorInit()
/* initialize monitoring on the first call */
{
unsigned flags = 0;
char *val;

/* there is special code in cheap.cgi to pass these from cgiOption to env */

val = getenv("JKSQL_TRACE");
if ((val != NULL) && sameString(val, "on"))
    flags |= JKSQL_TRACE;
val = getenv("JKSQL_PROF");
if ((val != NULL) && sameString(val, "on"))
    flags |= JKSQL_PROF;
if (flags != 0)
    sqlMonitorEnable(flags);

monitorInited = TRUE;
}

static void monitorEnter()
/* called at the beginning of a routine that is monitored, initialize if
 * necessary and start timing if enabled */
{
if (!monitorInited)
    monitorInit();
assert(monitorEnterTime == 0);  /* no recursion allowed */
if (monitorFlags)
    {
    monitorEnterTime = clock1000();
    }
}

static long monitorLeave()
/* called at the end of a routine that is monitored, updates time count.
 * returns time since enter. */
{
long deltaTime = 0;
if (monitorFlags)
    {
    deltaTime = clock1000() - monitorEnterTime;
    assert(monitorEnterTime > 0);
    if (monitorFlags & JKSQL_PROF)
        sqlTotalTime += deltaTime;
    monitorEnterTime = 0;
    }
return deltaTime;
}

static void monitorPrintInfo(struct sqlConnection *sc, char *name)
/* print a monitor message, with connection id and databases. */
{
fprintf(stderr, "%.*s%s %ld %s\n", traceIndent, indentStr, name,
        sc->conn->thread_id, sc->conn->db);
}

static void monitorPrint(struct sqlConnection *sc, char *name,
                         char *format, ...)
/* print a monitor message, with connection id, databases, and
 * printf style message.*/
{
va_list args;
fprintf(stderr, "%.*s%s %ld %s ", traceIndent, indentStr, name,
        sc->conn->thread_id, sc->conn->db);
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
}

static void monitorPrintTime()
/* print total time */
{
/* only print if not explictly disabled */
if (monitorFlags & JKSQL_PROF)
    {
    fprintf(stderr, "%.*sSQL_TOTAL_TIME %0.3fs\n", traceIndent, indentStr,
            ((double)sqlTotalTime)/1000.0);
    fprintf(stderr, "%.*sSQL_TOTAL_QUERIES %ld\n", traceIndent, indentStr,
            sqlTotalQueries);
    }
}

static void monitorPrintQuery(struct sqlConnection *sc, char *query)
/* print a query, replacing newlines with \n */
{
char *cleaned = replaceChars(query, "\n", "\\n");
monitorPrint(sc, "SQL_QUERY", "%s", cleaned);
freeMem(cleaned);
}

void sqlMonitorEnable(unsigned flags)
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
{
monitorFlags = flags;

if ((monitorFlags & JKSQL_PROF) && !monitorHandlerSet)
    {
    /* only add once */
    atexit(monitorPrintTime);
    monitorHandlerSet = TRUE;
    }

monitorInited = TRUE;
}

void sqlMonitorSetIndent(unsigned indent)
/* set the sql indent level indent to the number of spaces to indent each
 * trace, which can be helpful in making voluminous trace info almost
 * readable. */
{
traceIndent = indent;
}

void sqlMonitorDisable()
/* Disable tracing or profiling of SQL queries. */
{
if (monitorFlags & JKSQL_PROF)
    monitorPrintTime();

monitorFlags = 0;
sqlTotalTime = 0;  /* allow reenabling */
sqlTotalQueries = 0; 
}

void sqlFreeResult(struct sqlResult **pRes)
/* Free up a result. */
{
struct sqlResult *res = *pRes;
if (res != NULL)
    {
    if (res->result != NULL)
        {
        monitorEnter();
	mysql_free_result(res->result);
        monitorLeave();
        }
    if (res->node != NULL)
	{
	dlRemove(res->node);
	freeMem(res->node);
	}
    freez(pRes);
    }
}

void sqlDisconnect(struct sqlConnection **pSc)
/* Close down connection. */
{
struct sqlConnection *sc = *pSc;
long deltaTime;
if (sc != NULL)
    {
    MYSQL *conn = sc->conn;
    struct dlList *resList = sc->resultList;
    struct dlNode *node = sc->node;
    if (resList != NULL)
	{
	struct dlNode *resNode, *resNext;
	for (resNode = resList->head; resNode->next != NULL; resNode = resNext)
	    {
	    struct sqlResult *res = resNode->val;
	    resNext = resNode->next;
	    sqlFreeResult(&res);
	    }
	freeDlList(&resList);
	}
    if (conn != NULL)
	{
	if (sc->hasHardLock)
	    sqlHardUnlockAll(sc);
        if (monitorFlags & JKSQL_TRACE)
            monitorPrintInfo(sc, "SQL_DISCONNECT");
        monitorEnter();
	mysql_close(conn);

	deltaTime = monitorLeave();
	if (monitorFlags & JKSQL_TRACE)
	    monitorPrint(sc, "SQL_TIME", "%0.3fs", ((double)deltaTime)/1000.0);
	}
    if (node != NULL)
	{
	dlRemove(node);
	freeMem(node);
	}
    freez(pSc);
    }
}

char* sqlGetDatabase(struct sqlConnection *sc)
/* Get the database associated with an connection. */
{
return sc->conn->db;
}

struct slName *sqlGetAllDatabase(struct sqlConnection *sc)
/* Get a list of all database on the server */
{
struct sqlResult *sr = sqlGetResult(sc, "show databases");
char **row;
struct slName *databases = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    slSafeAddHead(&databases, slNameNew(row[0]));
    }
sqlFreeResult(&sr);
return databases;
}

struct slName *sqlListTables(struct sqlConnection *conn)
/* Return list of tables in database associated with conn. */
{
struct sqlResult *sr = sqlGetResult(conn, "show tables");
char **row;
struct slName *list = NULL, *el;
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct slName *sqlListFields(struct sqlConnection *conn, char *table)
/* Return list of fields in table. */
{
char query[256];
char **row;
struct slName *list = NULL, *el;
struct sqlResult *sr = NULL;
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

static void addDatabaseFields(char *database, struct hash *hash)
/* Add fields from the one database to hash. */
{
struct sqlConnection *conn = sqlConnect(database);
struct slName *table, *tableList = sqlListTables(conn);
struct sqlResult *sr;
char query[256];
char **row;
char fullName[512];
for (table = tableList; table != NULL; table = table->next)
    {
    safef(query, sizeof(query), "describe %s", table->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	safef(fullName, sizeof(fullName), "%s.%s.%s", 
	    database, table->name, row[0]);
	hashAdd(hash, fullName, NULL);
	}
    sqlFreeResult(&sr);
    }
slFreeList(&tableList);
sqlDisconnect(&conn);
}

struct hash *sqlAllFields()
/* Get hash of all fields in database.table.field format.  */
{
struct hash *fullHash = hashNew(18);
struct hash *dbHash = sqlHashOfDatabases();
struct hashEl *dbList, *db;
dbList = hashElListHash(dbHash);
for (db = dbList; db != NULL; db = db->next)
    addDatabaseFields(db->name, fullHash);
slFreeList(&dbList);
hashFree(&dbHash);
return fullHash;
}


void sqlCleanupAll()
/* Cleanup all open connections and resources. */
{
if (sqlOpenConnections)
    {
    struct dlNode *conNode, *conNext;
    struct sqlConnection *conn;
    for (conNode = sqlOpenConnections->head; conNode->next != NULL; conNode = conNext)
	{
	conn = conNode->val;
	conNext = conNode->next;
	sqlDisconnect(&conn);
	}
    freeDlList(&sqlOpenConnections);
    }
}

static void sqlInitTracking()
/* Initialize tracking and freeing of resources. */
{
if (sqlOpenConnections == NULL)
    {
    sqlOpenConnections = newDlList();
    atexit(sqlCleanupAll);
    }
}

struct sqlConnection *sqlConnRemote(char *host, 
	char *user, char *password, char *database, boolean abort)
/* Connect to database somewhere as somebody.  
 * If abort is set display error message and abort on error. */
{
struct sqlConnection *sc;
MYSQL *conn;
long deltaTime;

sqlInitTracking();

AllocVar(sc);
sc->resultList = newDlList();
sc->node = dlAddValTail(sqlOpenConnections, sc);

monitorEnter();
if ((sc->conn = conn = mysql_init(NULL)) == NULL)
    {
    monitorLeave();
    errAbort("Couldn't connect to mySQL.");
    }
if (mysql_real_connect(
	conn,
	host, /* host */
	user,	/* user name */
	password,	/* password */
	database, /* database */
	0,	/* port */
	NULL,	/* socket */
	0)	/* flags */  == NULL)
    {
    monitorLeave();
    if (abort)
	errAbort("Couldn't connect to database %s on %s as %s.\n%s", 
	    database, host, user, mysql_error(conn));
    return NULL;
    }
if (monitorFlags & JKSQL_TRACE)
    monitorPrint(sc, "SQL_CONNECT", "%s %s", host, user);

deltaTime = monitorLeave();
if (monitorFlags & JKSQL_TRACE)
    monitorPrint(sc, "SQL_TIME", "%0.3fs", ((double)deltaTime)/1000.0);
return sc;
}

struct sqlConnection *sqlConnectRemote(char *host, 
	char *user, char *password, char *database)
/* Connect to database somewhere as somebody. */
{
return sqlConnRemote(host, user, password, database, TRUE);
}

struct sqlConnection *sqlCtConn(boolean abort)
/* Connect to customTrash database, optionally abort on failure */
{
char *host = cfgOptionDefault("customTracks.host", NULL);
char *user = cfgOptionDefault("customTracks.user", NULL);
char *password = cfgOptionDefault("customTracks.password", NULL);

if (host && user && password)
    return sqlConnRemote(host, user, password, CUSTOM_TRASH, abort);
else
    {
    if (abort)
	errAbort("can not find customTracks DB conn info in hg.conf");
    else
	return NULL;
    }
return NULL;
}

struct sqlConnection *sqlConn(char *database, boolean abort)
/* Connect to database on default host as default user. 
 * Optionally abort on failure. */
{
char* host = getCfgValue("HGDB_HOST", "db.host");
char* user = getCfgValue("HGDB_USER", "db.user");
char* password = getCfgValue("HGDB_PASSWORD", "db.password");

if(host == 0 || user == 0 || password == 0)
    sqlConnectReadOnly(database);
return sqlConnRemote(host, user, password, database, abort);
}

struct sqlConnection *sqlMayConnect(char *database)
/* Connect to database on default host as default user. 
 * Return NULL (don't abort) on failure. */
{
return sqlConn(database, FALSE);
}

struct sqlConnection *sqlConnect(char *database)
/* Connect to database on default host as default user. */
{
return sqlConn(database, TRUE);
}

struct sqlConnection *sqlConnectReadOnly(char *database)
/* Connect to database on default host as default user. */
{
char* host = getCfgValue("HGDB_HOST", "ro.host");
char* user = getCfgValue("HGDB_USER", "ro.user");
char* password = getCfgValue("HGDB_PASSWORD", "ro.password");

if(host == 0 || user == 0 || password == 0)
    {
    errAbort("Could not read hostname, user, or password to the database from configuration file.");
    }
return sqlConnectRemote(host, user, password, database);
}

void sqlVaWarn(struct sqlConnection *sc, char *format, va_list args)
/* Default error message handler. */
{
MYSQL *conn = sc->conn;
if (format != NULL) {
    vaWarn(format, args);
    }
warn("mySQL error %d: %s", mysql_errno(conn), mysql_error(conn));
}

void sqlWarn(struct sqlConnection *sc, char *format, ...)
/* Printf formatted error message that adds on sql 
 * error message. */
{
va_list args;
va_start(args, format);
sqlVaWarn(sc, format, args);
va_end(args);
}

void sqlAbort(struct sqlConnection  *sc, char *format, ...)
/* Printf formatted error message that adds on sql 
 * error message and abort. */
{
va_list args;
va_start(args, format);
sqlVaWarn(sc, format, args);
va_end(args);
noWarnAbort();
}

typedef MYSQL_RES *	STDCALL ResGetter(MYSQL *mysql);

static struct sqlResult *sqlUseOrStore(struct sqlConnection *sc, 
	char *query, ResGetter *getter, boolean abort)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */
{
MYSQL *conn = sc->conn;
struct sqlResult *res = NULL;
long deltaTime;

++sqlTotalQueries; 

if (monitorFlags & JKSQL_TRACE)
    monitorPrintQuery(sc, query);

monitorEnter();
if (mysql_real_query(conn, query, strlen(query)) != 0)
    {
    if (abort)
        {
        monitorLeave();
	sqlAbort(sc, "Can't start query:\n%s\n", query);
        }
    }
else
    {
    MYSQL_RES *resSet;
    if ((resSet = getter(conn)) == NULL)
	{
	if (mysql_errno(conn) != 0)
	    {
            monitorLeave();
	    sqlAbort(sc, "Can't use query:\n%s", query);
	    }
	}
    else
        {
        AllocVar(res);
        res->conn = sc;
        res->result = resSet;
        res->node = dlAddValTail(sc->resultList, res);
        }
    }
deltaTime = monitorLeave();
if (monitorFlags & JKSQL_TRACE)
    monitorPrint(sc, "SQL_TIME", "%0.3fs", ((double)deltaTime)/1000.0);
return res;
}

void sqlDropTable(struct sqlConnection *sc, char *table)
/* Drop table if it exists. */
{
if (sqlTableExists(sc, table))
    {
    char query[256];
    safef(query, sizeof(query), "drop table %s", table);
    sqlUpdate(sc, query);
    }
}

void sqlGetLock(struct sqlConnection *sc, char *name)
/* Sets an advisory lock on the process for 1000s returns 1 if successful,*/
/* 0 if name already locked or NULL if error occurred */
/* blocks another client from obtaining a lock with the same name */
{
char query[256];
struct sqlResult *res;
char **row = NULL;
                                                                                
safef(query, sizeof(query), "select get_lock('%s', 1000)", name);
res = sqlGetResult(sc, query);
while ((row=sqlNextRow(res)))
    {
    if (sameWord(*row, "1"))
        printf("Advisory lock created\n");
    else if (sameWord(*row, "0"))
        errAbort("Attempt to GET_LOCK timed out.\nAnother client may have locked this name, %s\n.", name);
    else if (*row == NULL)
        errAbort("Attempt to GET_LOCK of name, %s, caused an error\n", name);
    }
sqlFreeResult(&res);
}

void sqlReleaseLock(struct sqlConnection *sc, char *name)
/* Releases an advisory lock created by GET_LOCK in sqlGetLock */
{
char query[256];
                                                                                
safef(query, sizeof(query), "select release_lock('%s')", name);
sqlUpdate(sc, query);
printf("Advisory lock has been released\n");
}

void sqlHardUnlockAll(struct sqlConnection *sc)
/* Unlock any hard locked tables. */
{
if (sc->hasHardLock)
    {
    sqlUpdate(sc, "unlock tables");
    sc->hasHardLock = FALSE;
    }
}

void sqlHardLockTables(struct sqlConnection *sc, struct slName *tableList, 
	boolean isWrite)
/* Hard lock given table list.  Unlock with sqlHardUnlockAll. */
{
struct dyString *dy = dyStringNew(0);
struct slName *table;
char *how = (isWrite ? "WRITE" : "READ");

if (sc->hasHardLock)
    errAbort("sqlHardLockTables repeated without sqlHardUnlockAll.");
dyStringAppend(dy, "LOCK TABLES ");
for (table = tableList; table != NULL; table = table->next)
    {
    dyStringPrintf(dy, "%s %s", table->name, how);
    if (table->next != NULL)
       dyStringAppendC(dy, ',');
    }
sqlUpdate(sc, dy->string);

sc->hasHardLock = TRUE;
dyStringFree(&dy);
}

void sqlHardLockTable(struct sqlConnection *sc, char *table, boolean isWrite)
/* Lock a single table.  Unlock with sqlHardUnlockAll. */
{
struct slName *list = slNameNew(table);
sqlHardLockTables(sc, list, isWrite);
slFreeList(&list);
}

void sqlHardLockAll(struct sqlConnection *sc, boolean isWrite)
/* Lock all tables in current database.  Unlock with sqlHardUnlockAll. */
{
struct slName *tableList =  sqlListTables(sc);
sqlHardLockTables(sc, tableList, isWrite);
slFreeList(&tableList);
}
                                                                                
boolean sqlMaybeMakeTable(struct sqlConnection *sc, char *table, char *query)
/* Create table from query if it doesn't exist already.
 * Returns FALSE if didn't make table. */
{
if (sqlTableExists(sc, table))
    return FALSE;
sqlUpdate(sc, query);
return TRUE;
}

void sqlRemakeTable(struct sqlConnection *sc, char *table, char *create)
/* Drop table if it exists, and recreate it. */
{
sqlDropTable(sc, table);
sqlUpdate(sc, create);
}

boolean sqlDatabaseExists(char *database)
/* Return TRUE if database exists. */
{
struct sqlConnection *conn = sqlMayConnect(database);
boolean exists = (conn != NULL);
sqlDisconnect(&conn);
return exists;
}

boolean sqlTableExists(struct sqlConnection *sc, char *table)
/* Return TRUE if a table exists. */
{
char query[256];
struct sqlResult *sr;

safef(query, sizeof(query), "select count(*) from %s", table);
if ((sr = sqlUseOrStore(sc,query,mysql_use_result, FALSE)) == NULL)
    return FALSE;
sqlNextRow(sr);	/* Just discard. */
sqlFreeResult(&sr);
return TRUE;
}

int sqlTableSizeIfExists(struct sqlConnection *sc, char *table)
/* Return row count if a table exists, -1 if it doesn't. */
{
char query[256];
struct sqlResult *sr;
char **row = 0;
int ret = 0;

safef(query, sizeof(query), "select count(*) from %s", table);
if ((sr = sqlUseOrStore(sc,query,mysql_use_result, FALSE)) == NULL)
    return -1;
row = sqlNextRow(sr);
if (row != NULL && row[0] != NULL)
    ret = atoi(row[0]);
sqlFreeResult(&sr);
return ret;
}

boolean sqlTablesExist(struct sqlConnection *conn, char *tables)
/* Check all tables in space delimited string exist. */
{
char *dupe = cloneString(tables);
char *s = dupe, *word;
boolean ok = TRUE;
while ((word = nextWord(&s)) != NULL)
     {
     if (!sqlTableExists(conn, word))
         {
	 ok = FALSE;
	 break;
	 }
     }
freeMem(dupe);
return ok;
}

boolean sqlTableWildExists(struct sqlConnection *sc, char *table)
/* Return TRUE if table (which can include SQL wildcards) exists. 
 * A bit slower than sqlTableExists. */
{
char query[512];
struct sqlResult *sr;
char **row;
boolean exists;

safef(query, sizeof(query), "show tables like '%s'", table);
sr = sqlGetResult(sc, query);
exists = ((row = sqlNextRow(sr)) != NULL);
sqlFreeResult(&sr);
return exists;
}

static char **sqlMaybeNextRow(struct sqlResult *sr, boolean *retOk)
/* Get next row from query result; set retOk according to error status. */
{
char** row = NULL;
if (sr != NULL)
    {
    monitorEnter();
    row = mysql_fetch_row(sr->result);
    monitorLeave();
    if (mysql_errno(sr->conn->conn) != 0)
	{
	if (retOk != NULL)
	    *retOk = FALSE;
	}
    else if (retOk != NULL)
	*retOk = TRUE;
    }
else if (retOk != NULL)
    *retOk = TRUE;
return row;
}

boolean sqlTableOk(struct sqlConnection *sc, char *table)
/* Return TRUE if a table not only exists, but also is not corrupted. */
{
int rowCount = sqlTableSizeIfExists(sc, table);
if (rowCount < 0)
    return FALSE;
else if (rowCount == 0)
    return TRUE;
else
    {
    char query[256];
    struct sqlResult *sr;
    boolean ret = TRUE;
    safef(query, sizeof(query), "select * from %s limit 1,1", table);
    if ((sr = sqlUseOrStore(sc,query,mysql_use_result, FALSE)) == NULL)
	ret = FALSE;
    else
	/* Discard row, but see if we get an error while reading it. */
	sqlMaybeNextRow(sr, &ret);
    sqlFreeResult(&sr);
    return ret;
    }
}

struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on. */
{
return sqlUseOrStore(sc,query,mysql_use_result, TRUE);
}

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query)
/* Query database. If result empty squawk and die. */
{
struct sqlResult *res = sqlGetResult(sc,query);
if (res == NULL)
	errAbort("Object not found in database.\nQuery was %s", query);
return res;
}


void sqlUpdate(struct sqlConnection *conn, char *query)
/* Tell database to do something that produces no results table. */
{
struct sqlResult *sr;
sr = sqlGetResult(conn,query);
sqlFreeResult(&sr);
}

int sqlUpdateRows(struct sqlConnection *conn, char *query, int* matched)
/* Execute an update query, returning the number of rows change.  If matched
 * is not NULL, it gets the total number matching the query. */
{
int numChanged, numMatched;
const char *info;
int numScan = 0;
struct sqlResult *sr = sqlGetResult(conn,query);

/* Rows matched: 40 Changed: 40 Warnings: 0 */
monitorEnter();
info = mysql_info(conn->conn);
monitorLeave();
if (info != NULL)
    numScan = sscanf(info, "Rows matched: %d Changed: %d Warnings: %*d",
                     &numMatched, &numChanged);
if ((info == NULL) || (numScan < 2))
    errAbort("can't get info (maybe not an sql UPDATE): %s", query);
sqlFreeResult(&sr);
if (matched != NULL)
    *matched = numMatched;
return numChanged;
}

static boolean isMySql4(struct sqlConnection *conn)
/* determine if this is at least mysql 4.0 or newer */
{
char majorVerBuf[64];
char *dotPtr = strchr(conn->conn->server_version, '.');
int len = (dotPtr - conn->conn->server_version);
assert(dotPtr != NULL);
strncpy(majorVerBuf, conn->conn->server_version, len);
majorVerBuf[len] = '\0';

return (sqlUnsigned(majorVerBuf) >= 4);
}

void sqlLoadTabFile(struct sqlConnection *conn, char *path, char *table,
                    unsigned options)
/* Load a tab-seperated file into a database table, checking for errors. 
 * Options are the SQL_TAB_* bit set. */
{
char tabPath[PATH_LEN];
char query[PATH_LEN+256];
int numScan, numRecs, numSkipped, numWarnings;
char *localOpt, *concurrentOpt, *dupOpt;
const char *info;
struct sqlResult *sr;
boolean mysql4 = isMySql4(conn);

/* Doing an "alter table disable keys" command implicitly commits the current
   transaction. Don't want to use that optimization if we need to be transaction
   safe. */
/* FIXME: markd 2003/01/05: mysql 4.0.17 - the alter table enable keys hangs,
 * disable this optimization for now. Verify performance on small loads
 * before re-enabling*/
# if 0
boolean doDisableKeys = !(options & SQL_TAB_TRANSACTION_SAFE);
#else
boolean doDisableKeys = FALSE;
#endif

/* determine if tab file can be accessed directly by the database, or send
 * over the network */
if (options & SQL_TAB_FILE_ON_SERVER)
    {
    /* tab file on server requiries full path */
    strcpy(tabPath, "");
    if (path[0] != '/')
        {
        getcwd(tabPath, sizeof(tabPath));
        strcat(tabPath, "/");
        }
    strcat(tabPath, path);
    localOpt = "";
    }
else
    {
    strcpy(tabPath, path);
    localOpt = "LOCAL";
    }

/* optimize for concurrent to others to access the table. */
if (options & SQL_TAB_FILE_CONCURRENT)
    concurrentOpt = "CONCURRENT";
else
    {
    concurrentOpt = "";
    if (mysql4 && doDisableKeys)
        {
        /* disable update of indexes during load. Inompatible with concurrent,
         * since enable keys locks other's out. */
        safef(query, sizeof(query), "ALTER TABLE %s DISABLE KEYS", table);
        sqlUpdate(conn, query);
        }
    }

if (options & SQL_TAB_REPLACE)
    dupOpt = "REPLACE";
else
    dupOpt = "";

safef(query, sizeof(query),  "LOAD DATA %s %s INFILE '%s' %s INTO TABLE %s",
      concurrentOpt, localOpt, tabPath, dupOpt, table);
sr = sqlGetResult(conn, query);
monitorEnter();
info = mysql_info(conn->conn);
monitorLeave();
if (info == NULL)
    errAbort("no info available for result of sql query: %s", query);
numScan = sscanf(info, "Records: %d Deleted: %*d  Skipped: %d  Warnings: %d",
                 &numRecs, &numSkipped, &numWarnings);
if (numScan != 3)
    errAbort("can't parse sql load info: %s", info);
sqlFreeResult(&sr);

if ((numSkipped > 0) || (numWarnings > 0))
    {
    boolean doAbort = TRUE;
    if ((numSkipped > 0) && (options & SQL_TAB_FILE_WARN_ON_ERROR))
        doAbort = FALSE;  /* don't abort on errors */
    else if ((numWarnings > 0) &&
             (options & (SQL_TAB_FILE_WARN_ON_ERROR|SQL_TAB_FILE_WARN_ON_WARN)))
        doAbort = FALSE;  /* don't abort on warnings */

    if (doAbort)
        errAbort("load of %s did not go as planned: %d record(s), "
                 "%d row(s) skipped, %d warning(s) loading %s",
                 table, numRecs, numSkipped, numWarnings, path);
    else
        warn("Warning: load of %s did not go as planned: %d record(s), "
             "%d row(s) skipped, %d warning(s) loading %s",
             table, numRecs, numSkipped, numWarnings, path);
    }
if (((options & SQL_TAB_FILE_CONCURRENT) == 0) && mysql4 && doDisableKeys)
    {
    /* reenable update of indexes */
    safef(query, sizeof(query), "ALTER TABLE %s ENABLE KEYS", table);
    sqlUpdate(conn, query);
    }
}

boolean sqlExists(struct sqlConnection *conn, char *query)
/* Query database and return TRUE if it had a non-empty result. */
{
struct sqlResult *sr;
if ((sr = sqlGetResult(conn,query)) == NULL)
    return FALSE;
else
    {
    if(sqlNextRow(sr) == NULL)
	{
	sqlFreeResult(&sr);
	return FALSE;
	}
    else
	{
	sqlFreeResult(&sr);
	return TRUE;
	}
    }
}

boolean sqlRowExists(struct sqlConnection *conn,
	char *table, char *field, char *key)
/* Return TRUE if row where field = key is in table. */
{
char query[256];
safef(query, sizeof(query), "select count(*) from %s where %s = '%s'",
	table, field, key);
return sqlQuickNum(conn, query) > 0;
}


struct sqlResult *sqlStoreResult(struct sqlConnection *sc, char *query)
/* Returns NULL if result was empty.  Otherwise returns a structure
 * that you can do sqlRow() on.  Same interface as sqlGetResult,
 * but internally this keeps the entire result in memory. */
{
return sqlUseOrStore(sc,query,mysql_store_result, TRUE);
}

char **sqlNextRow(struct sqlResult *sr)
/* Get next row from query result. */
{
boolean ok = FALSE;
char** row = sqlMaybeNextRow(sr, &ok);
if (! ok)
    sqlAbort(sr->conn, "nextRow failed");
return row;
}

char* sqlFieldName(struct sqlResult *sr)
/* repeated calls to this function returns the names of the fields 
 * the given result */
{
MYSQL_FIELD *field;
field = mysql_fetch_field(sr->result);
if(field == NULL)
    return NULL;
return field->name;
}

int sqlFieldColumn(struct sqlResult *sr, char *colName)
/* get the column number of the specified field in the result, or
 * -1 if the result doesn't contailed the field.*/
{
int numFields = mysql_num_fields(sr->result);
int i;
for (i = 0; i < numFields; i++)
    {
    MYSQL_FIELD *field = mysql_fetch_field_direct(sr->result, i);
    if (sameString(field->name, colName))
        return i;
    }
return -1;
}

int sqlCountColumns(struct sqlResult *sr)
/* Count the number of columns in result. */
{
if(sr != NULL)
    return mysql_field_count(sr->conn->conn);
return 0;
}

#ifdef SOMETIMES  /* Not available for all MYSQL environments. */
int sqlFieldCount(struct sqlResult *sr)
/* Return number of fields in a row of result. */
{
if (sr == NULL)
    return 0;
return mysql_field_count(sr->result);
}
#endif /* SOMETIMES */

int sqlFieldCount(struct sqlResult *sr)
/* Return number of fields in a row of result. */
{
if (sr == NULL)
    return 0;
return mysql_num_fields(sr->result);
}

int sqlCountColumnsInTable(struct sqlConnection *sc, char *table)
/* Return the number of columns in a table */
{
char query[256];
struct sqlResult *sr;
char **row;
int count;

/* Read table description and count rows. */
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(sc, query);
count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    }
sqlFreeResult(&sr);
return count;
}

char *sqlQuickQuery(struct sqlConnection *sc, char *query, char *buf, int bufSize)
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Returns NULL if query comes up empty. */
{
struct sqlResult *sr;
char **row;
char *ret = NULL;

if ((sr = sqlGetResult(sc, query)) == NULL)
    return NULL;
row = sqlNextRow(sr);
if (row != NULL && row[0] != NULL)
    {
    strncpy(buf, row[0], bufSize);
    ret = buf;
    }
sqlFreeResult(&sr);
return ret;
}

char *sqlNeedQuickQuery(struct sqlConnection *sc, char *query, 
	char *buf, int bufSize)
/* Does query and returns first field in first row.  Meant
 * for cases where you are just looking up one small thing.  
 * Prints error message and aborts if query comes up empty. */
{
char *s = sqlQuickQuery(sc, query, buf, bufSize);
if (s == NULL)
    errAbort("query not found: %s", query);
return s;
}


int sqlQuickNum(struct sqlConnection *conn, char *query)
/* Get numerical result from simple query */
{
struct sqlResult *sr;
char **row;
int ret = 0;

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL && row[0] != NULL)
    ret = atoi(row[0]);
sqlFreeResult(&sr);
return ret;
}

double sqlQuickDouble(struct sqlConnection *conn, char *query)
/* Get floating point numerical result from simple query */
{
struct sqlResult *sr;
char **row;
double ret = 0;

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL && row[0] != NULL)
    ret = atof(row[0]);
sqlFreeResult(&sr);
return ret;
}


int sqlNeedQuickNum(struct sqlConnection *conn, char *query)
/* Get numerical result or die trying. */
{
char buf[32];
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
if (!((buf[0] == '-' && isdigit(buf[1])) || isdigit(buf[0])))
    errAbort("Expecting numerical result to query '%s' got '%s'",
    	query, buf);
return sqlSigned(buf);
}

char *sqlQuickString(struct sqlConnection *sc, char *query)
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd. */
{
struct sqlResult *sr;
char **row;
char *ret = NULL;

if ((sr = sqlGetResult(sc, query)) == NULL)
    return NULL;
row = sqlNextRow(sr);
if (row != NULL && row[0] != NULL)
    ret = cloneString(row[0]);
sqlFreeResult(&sr);
return ret;
}

char *sqlNeedQuickString(struct sqlConnection *sc, char *query)
/* Return result of single-row/single column query in a
 * string that should eventually be freeMem'd.  This will
 * print an error message and abort if result returns empty. */
{
char *s = sqlQuickString(sc, query);
if (s == NULL)
    errAbort("query not found: %s", query);
return s;
}

char *sqlQuickNonemptyString(struct sqlConnection *conn, char *query)
/* Return first result of given query.  If it is an empty string
 * convert it to NULL. */
{
char *result = sqlQuickString(conn, query);
if (result != NULL && result[0] == 0)
    freez(&result);
return result;
}

struct slName *sqlQuickList(struct sqlConnection *conn, char *query)
/* Return a list of slNames for a single column query.
 * Do a slFreeList on result when done. */
{
struct slName *list = NULL, *n;
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    n = slNameNew(row[0]);
    slAddHead(&list, n);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct hash *sqlQuickHash(struct sqlConnection *conn, char *query)
/* Return a hash filled with results of two column query. 
 * The first column is the key, the second the value. */
{
struct hash *hash = hashNew(16);
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], cloneString(row[1]));
sqlFreeResult(&sr);
return hash;
}

struct slInt *sqlQuickNumList(struct sqlConnection *conn, char *query)
/* Return a list of slInts for a single column query.
 * Do a slFreeList on result when done. */
{
struct slInt *list = NULL, *n;
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    n = slIntNew(sqlSigned(row[0]));
    slAddHead(&list, n);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct slDouble *sqlQuickDoubleList(struct sqlConnection *conn, char *query)
/* Return a list of slDoubles for a single column query.
 * Do a slFreeList on result when done. */
{
struct slDouble *list = NULL, *n;
struct sqlResult *sr;
char **row;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    n = slDoubleNew(atof(row[0]));
    slAddHead(&list, n);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}


int sqlTableSize(struct sqlConnection *conn, char *table)
/* Find number of rows in table. */
{
char query[128];
safef(query, sizeof(query), "select count(*) from %s", table);
return sqlQuickNum(conn, query);
}

int sqlFieldIndex(struct sqlConnection *conn, char *table, char *field)
/* Returns index of field in a row from table, or -1 if it 
 * doesn't exist. */
{
char query[256];
struct sqlResult *sr;
char **row;
int i = 0, ix=-1;

/* Read table description into hash. */
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], field))
        {
	ix = i;
	break;
	}
    ++i;
    }
sqlFreeResult(&sr);
return ix;
}


unsigned int sqlLastAutoId(struct sqlConnection *conn)
/* Return last automatically incremented id inserted into database. */
{
unsigned id;
monitorEnter();
id = mysql_insert_id(conn->conn);
monitorLeave();
return id;
}

/* Stuff to manage and cache up to 16 open connections on 
 * a database.  Typically you only need 3.
 * MySQL takes about 2 milliseconds on a local
 * host to open a connection.  On a remote host it can be more
 * and this caching is probably actually necessary. */

enum {maxConn = 16};

struct sqlConnCache
    {
    char *database;				/* SQL database. */
    char *host;					/* Host machine of database. */
    char *user;					/* Database user name */
    char *password;				/* Password. */
    int connAlloced;                            /* # open connections. */
    struct sqlConnection *connArray[maxConn];   /* Open connections. */
    boolean connUsed[maxConn];                  /* Tracks used conns. */
    };

struct sqlConnCache *sqlNewRemoteConnCache(char *database, 
	char *host, char *user, char *password)
/* Set up a cache on a remote database. */
{
struct sqlConnCache *cache;
AllocVar(cache);
cache->database = cloneString(database);
cache->host = cloneString(host);
cache->user = cloneString(user);
cache->password = cloneString(password);
return cache;
}

struct sqlConnCache *sqlNewConnCache(char *database)
/* Return a new connection cache. */
{
char* host = getCfgValue("HGDB_HOST", "db.host");
char* user = getCfgValue("HGDB_USER", "db.user");
char* password = getCfgValue("HGDB_PASSWORD", "db.password");
if (password == NULL || user == NULL || host == NULL)
    errAbort("Could not read hostname, user, or password to the database from configuration file.");
return sqlNewRemoteConnCache(database, host, user, password);
}

void sqlFreeConnCache(struct sqlConnCache **pCache)
/* Dispose of a connection cache. */
{
struct sqlConnCache *cache;
if ((cache = *pCache) != NULL)
    {
    int i;
    for (i=0; i<cache->connAlloced; ++i)
	sqlDisconnect(&cache->connArray[i]);
    freeMem(cache->database);
    freeMem(cache->host);
    freeMem(cache->user);
    freeMem(cache->password);
    freez(pCache);
    }
}

struct sqlConnection *sqlMayAllocConnection(struct sqlConnCache *cache,
					    boolean mustConnect)
/* Allocate a cached connection. errAbort if too many open connections.  
 * errAbort if mustConnect and connection fails. */
{
int i;
int connAlloced = cache->connAlloced;
boolean *connUsed = cache->connUsed;
for (i=0; i<connAlloced; ++i)
    {
    if (!connUsed[i])
	{
	connUsed[i] = TRUE;
	return cache->connArray[i];
	}
    }
if (connAlloced >= maxConn)
   errAbort("Too many open sqlConnections to %s for cache", cache->database);
cache->connArray[connAlloced] = sqlConnRemote(cache->host, cache->user,
					      cache->password, cache->database,
					      mustConnect);
if (cache->connArray[connAlloced] != NULL)
    {
    connUsed[connAlloced] = TRUE;
    ++cache->connAlloced;
    }
return cache->connArray[connAlloced];
}

struct sqlConnection *sqlAllocConnection(struct sqlConnCache *cache)
/* Allocate a cached connection. */
{
return sqlMayAllocConnection(cache, TRUE);
}

void sqlFreeConnection(struct sqlConnCache *cache, struct sqlConnection **pConn)
/* Free up a cached connection. */
{
struct sqlConnection *conn;
int connAlloced = cache->connAlloced;
struct sqlConnection **connArray = cache->connArray;
boolean gotIt = FALSE;

if ((conn = *pConn) != NULL)
    {
    int ix;
    for (ix = 0; ix < connAlloced; ++ix)
	{
	if (connArray[ix] == conn)
	    {
	    cache->connUsed[ix] = FALSE;
	    gotIt = TRUE;
	    break;
	    }
	}
    if (! gotIt)
	errAbort("sqlFreeConnection called on cache (%s) that doesn't contain "
		 "the given connection", cache->database);
    *pConn = NULL;
    }
}


char *sqlEscapeString2(char *to, const char* from)
/* Prepares a string for inclusion in a sql statement.  Output string
 * must be 2*strlen(from)+1 */
{
mysql_escape_string(to, from, strlen(from));
return to; 
}

char *sqlEscapeString(const char* from)
/* Prepares string for inclusion in a SQL statement . Remember to free
 * returned string.  Returned string contains strlen(length)*2+1 as many bytes
 * as orig because in worst case every character has to be escaped.*/
{
int size = (strlen(from)*2) +1;
char *to = needMem(size * sizeof(char));
return sqlEscapeString2(to, from);
}

char *sqlEscapeTabFileString2(char *to, const char *from)
/* Escape a string for including in a tab seperated file. Output string
 * must be 2*strlen(from)+1 */
{
const char *fp = from;
char *tp = to;
while (*fp != '\0')
    {
    switch (*fp)
        {
        case '\\':
            *tp++ = '\\';
            *tp++ = '\\';
            break;
        case '\n':
            *tp++ = '\\';
            *tp++ = 'n';
            break;
        case '\t':
            *tp++ = '\\';
            *tp++ = 't';
            break;
        default:
            *tp++ = *fp;
            break;
        }
    fp++;
    }
*tp = '\0'; 
return to; 
}

char *sqlEscapeTabFileString(const char *from)
/* Escape a string for including in a tab seperated file. Output string
 * must be 2*strlen(from)+1 */
{
int size = (strlen(from)*2) +1;
char *to = needMem(size * sizeof(char));
return sqlEscapeTabFileString2(to, from);
}


struct hash *sqlHashOfDatabases()
/* Get hash table with names of all databases that are online. */
{
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect("mysql");
struct hash *hash = newHash(8);

sr = sqlGetResult(conn, "show databases");
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!startsWith("mysql", row[0]))  /* Avoid internal databases. */
	hashAdd(hash, row[0], NULL);
    }
sqlDisconnect(&conn);
return hash;
}

struct slName *sqlListOfDatabases()
/* Get list of all databases that are online. */
{
struct sqlConnection *conn = sqlConnect("mysql");
struct slName *list = sqlGetAllDatabase(conn);
sqlDisconnect(&conn);
return list;
}

char *connGetDatabase(struct sqlConnCache *conn)
/* return database for a connection cache */
{
return conn->database;
}

boolean sqlWildcardIn(char *s)
/* Return TRUE if there is a sql wildcard char in string. */
{
char c;
while ((c = *s++) != 0)
    {
    if (c == '_' || c == '%')
        return TRUE;
    }
return FALSE;
}

char *sqlLikeFromWild(char *wild)
/* Convert normal wildcard string to SQL wildcard by
 * mapping * to % and ? to _.  Escape any existing % and _'s. */
{
int escCount = countChars(wild, '%') + countChars(wild, '_');
int size = strlen(wild) + escCount + 1;
char *retVal = needMem(size);
char *s = retVal, c;

while ((c = *wild++) != 0)
    {
    switch (c)
	{
	case '%':
	case '_':
	    *s++ = '\\';
	    *s++ = c;
	    break;
	case '*':
	    *s++ = '%';
	    break;
	case '?':
	    *s++ = '_';
	    break;
	default:
	    *s++ = c;
	    break;
	}
    }
return retVal;
}

int sqlDateToUnixTime(char *sqlDate)
/* Convert a SQL date such as "2003-12-09 11:18:43" to clock time 
 * (seconds since midnight 1/1/1970 in UNIX). */
{
struct tm *tm = NULL;
long clockTime = 0;

if (sqlDate == NULL)
    errAbort("Null string passed to sqlDateToClockTime()");
AllocVar(tm);
if (sscanf(sqlDate, "%4d-%2d-%2d %2d:%2d:%2d",
	   &(tm->tm_year), &(tm->tm_mon), &(tm->tm_mday),
	   &(tm->tm_hour), &(tm->tm_min), &(tm->tm_sec))  != 6)
    errAbort("Couldn't parse sql date \"%s\"", sqlDate);
tm->tm_year -= 1900;
tm->tm_mon  -= 1;
clockTime = mktime(tm);
if (clockTime < 0)
    errAbort("mktime failed (%d-%d-%d %d:%d:%d).",
	     tm->tm_year, tm->tm_mon, tm->tm_mday,
	     tm->tm_hour, tm->tm_min, tm->tm_sec);
freez(&tm);
return clockTime;
}

char *sqlUnixTimeToDate(time_t *timep, boolean gmTime)
/* Convert a clock time (seconds since 1970-01-01 00:00:00 unix epoch)
 *	to the string: "YYYY-MM-DD HH:MM:SS"
 *  returned string is malloced, can be freed after use
 *  boolean gmTime requests GMT time instead of local time
 */
{
struct tm *tm;
char *ret;

if (gmTime)
    tm = gmtime(timep);
else
    tm = localtime(timep);

ret = (char *)needMem(25*sizeof(char));  /* 25 is good for a billion years */

snprintf(ret, 25*sizeof(char), "%d-%02d-%02d %02d:%02d:%02d",
    1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday,
    tm->tm_hour, tm->tm_min, tm->tm_sec);
return(ret);
}

static int getUpdateFieldIndex(struct sqlResult *sr)
/* Return index of update field. */
{
static int updateFieldIndex = -1;
if (updateFieldIndex < 0)
    {
    int ix;
    char *name;
    for (ix=0; ;++ix)
        {
	name = sqlFieldName(sr);
	if (name == NULL)
	    errAbort("Can't find Update_time field in show table status result");
	if (sameString("Update_time", name))
	    {
	    updateFieldIndex = ix;
	    break;
	    }
	}
    }
return updateFieldIndex;
}

char *sqlTableUpdate(struct sqlConnection *conn, char *table)
/* Get last update time for table as an SQL string */
{
char query[512], **row;
struct sqlResult *sr;
int updateIx;
char *ret;
safef(query, sizeof(query), "show table status like '%s'", table);
sr = sqlGetResult(conn, query);
updateIx = getUpdateFieldIndex(sr);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Database table %s doesn't exist", table);
ret = cloneString(row[updateIx]);
sqlFreeResult(&sr);
return ret;
}

int sqlTableUpdateTime(struct sqlConnection *conn, char *table)
/* Get last update time for table. */
{
char *date = sqlTableUpdate(conn, table);
int time = sqlDateToUnixTime(date);
freeMem(date);
return time;
}

char *sqlGetPrimaryKey(struct sqlConnection *conn, char *table)
/* Get primary key if any for table, return NULL if none. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *key = NULL;
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameWord(row[3], "PRI"))
	{
        key = cloneString(row[0]);
	break;
	}
    }
sqlFreeResult(&sr);
return key;
}

char *sqlVersion(struct sqlConnection *conn)
/* Return version of MySQL database.  This will be something
 * of the form 5.0.18-standard. */
{
char **row;
struct sqlResult *sr = sqlGetResult(conn, "show variables like 'version'");
char *version = NULL;
if ((row = sqlNextRow(sr)) != NULL)
    version = cloneString(row[1]);
else
    errAbort("No mySQL version var.");
sqlFreeResult(&sr);
return version;
}
    
int sqlMajorVersion(struct sqlConnection *conn)
/* Return major version of database. */
{
char *s = sqlVersion(conn);
int ver;
if (!isdigit(s[0]))
    errAbort("Unexpected format in version: %s", s);
ver = atoi(s);		/* NOT sqlUnsigned please! */
freeMem(s);
return ver;
}

int sqlMinorVersion(struct sqlConnection *conn)
/* Return minor version of database. */
{
char *s = sqlVersion(conn);
char *words[5];
int wordCount;
int ver;

wordCount = chopString(s, ".", words, ArraySize(words));

if (!isdigit(*words[1]))
    errAbort("Unexpected format in version: %s", s);
ver = atoi(words[1]);           /* NOT sqlUnsigned please! */
freeMem(s);
return ver;
}

char** sqlGetEnumDef(struct sqlConnection *conn, char* table, char* colName)
/* Get the definitions of a enum column in a table, returning a
 * null-terminated array of enum values.  Free array when finished.  */
{
static char *enumPrefix = "enum(";
struct sqlResult *sr;
char query[128];
char **row;
char *defStr, *defStrCp;
int numValues, i;
char **enumDef;

/* get enum definition */
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while (((row = sqlNextRow(sr)) != NULL) && !sameString(row[0], colName))
    continue;
if (row == NULL)
    errAbort("can't find column %s in results of %s", colName, query);

/* parse definition in the form:
 * enum('unpicked','candidate',... ,'cantSequence') */
if (!startsWith(enumPrefix, row[1]))
    errAbort("%s column %s isn't an enum: %s", table, colName, row[1]);
defStr = row[1] + strlen(enumPrefix);

/* build char** array with string space in same block */
numValues = chopString(defStr, ",", NULL, 0);
enumDef = needMem(((numValues+1) * sizeof (char**)) + strlen(defStr)+1);
defStrCp = ((char*)enumDef) + ((numValues+1) * sizeof (char**));
strcpy(defStrCp, defStr);
chopString(defStrCp, ",", enumDef, numValues);

/* remove quotes */
for (i = 0; enumDef[i] != NULL; i++)
    {
    int len = strlen(enumDef[i]);
    if (enumDef[i+1] == NULL)
        len--;  /* last entry hash close paren */
    if ((enumDef[i][0] != '\'') || (enumDef[i][len-1] != '\''))
        errAbort("can't find quotes in %s column %s enum value: %s", 
                 table, colName, enumDef[i]);
    enumDef[i][len-1] = '\0';
    enumDef[i]++;
    }

sqlFreeResult(&sr);
return enumDef;
}

struct slName *sqlRandomSampleWithSeedConn(struct sqlConnection *conn, char *table, char *field, int count, int seed)
/* Get random sample from database specifiying rand number seed, or -1 for none */
{
char query[256], **row;
struct sqlResult *sr;
struct slName *list = NULL, *el;
char seedString[256] = "";
/* The randomized-order, distinct-ing query can take a very long time on 
 * very large tables.  So create a smaller temporary table and use that. 
 * The temporary table is visible only to the current connection, so 
 * doesn't have to be very uniquely named, and will disappear when the 
 * connection is closed. */
safef(query, sizeof(query),
      "create temporary table tmp%s select %s from %s limit 100000",
      table, field, table);
sqlUpdate(conn, query);
if (seed != -1)
    safef(seedString,sizeof(seedString),"%d",seed);
safef(query, sizeof(query), "select distinct %s from tmp%s "
      "order by rand(%s) limit %d", 
      field, table, seedString, count);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
return list;
}

struct slName *sqlRandomSampleWithSeed(char *db, char *table, char *field, int count, int seed)
/* Get random sample from database specifiying rand number seed, or -1 for none */
{
struct sqlConnection *conn = sqlConnect(db);
return sqlRandomSampleWithSeedConn(conn, table, field, count, seed);
sqlDisconnect(&conn);
}

struct slName *sqlRandomSampleConn(struct sqlConnection *conn, char *table,
				   char *field, int count)
/* Get random sample from conn. */
{
return sqlRandomSampleWithSeedConn(conn, table, field, count, -1);
}

struct slName *sqlRandomSample(char *db, char *table, char *field, int count)
/* Get random sample from database. */
{
return sqlRandomSampleWithSeed(db, table, field, count, -1);
}


static struct sqlFieldInfo *sqlFieldInfoParse(char **row)
/* parse a row into a sqlFieldInfo object */
{
struct sqlFieldInfo *fi;
AllocVar(fi);
fi->field = cloneString(row[0]);
fi->type = cloneString(row[1]);
fi->allowsNull = sameString(row[2], "YES");
fi->key = cloneString(row[3]);
fi->defaultVal = cloneString(row[4]);
fi->extra = cloneString(row[5]);
return fi;
}

struct sqlFieldInfo *sqlFieldInfoGet(struct sqlConnection *conn, char *table)
/* get a list of objects describing the fields of a table */
{
char query[512];
struct sqlResult *sr;
char **row;
struct sqlFieldInfo *fiList = NULL;

safef(query, sizeof(query), "SHOW COLUMNS FROM %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slSafeAddHead(&fiList, sqlFieldInfoParse(row));
sqlFreeResult(&sr);
slReverse(&fiList);
return fiList;
}

static void sqlFieldInfoFree(struct sqlFieldInfo **fiPtr)
/* Free a sqlFieldInfo object */
{
struct sqlFieldInfo *fi = *fiPtr;
if (fi != NULL)
    {
    freeMem(fi->field);
    freeMem(fi->type);
    freeMem(fi->key); 
    freeMem(fi->defaultVal);
    freeMem(fi->extra);
    freeMem(fi);
    *fiPtr = NULL;
    }
}

void sqlFieldInfoFreeList(struct sqlFieldInfo **fiListPtr)
/* Free a list of sqlFieldInfo objects */
{
struct sqlFieldInfo *fi;
while ((fi = slPopHead(fiListPtr)) != NULL)
       sqlFieldInfoFree(&fi);
}

void *sqlVaQueryObjs(struct sqlConnection *conn, sqlLoadFunc loadFunc,
                     unsigned opts, char *queryFmt, va_list args)
/* Generate a query from format and args.  Load one or more objects from rows
 * using loadFunc.  Check the number of rows returned against the sqlQueryOpts
 * bit set.  Designed for use with autoSql, although load function must be
 * cast to sqlLoadFunc. */
{
char query[1024];
struct slList *objs = NULL;
struct sqlResult *sr;
char **row;

vasafef(query, sizeof(query), queryFmt, args);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL) {
slSafeAddHead(&objs, loadFunc(row));
}
sqlFreeResult(&sr);
slReverse(&objs);

/* check what we got against the options */
if (objs == NULL)
    {
    if (opts & sqlQueryMust)
        errAbort("no results return from query: %s", query);
    }
else if ((opts & sqlQuerySingle) && (objs->next != NULL))
    errAbort("one results, got %d, from query: %s", slCount(objs), query);
return objs;
}

/* Generate a query from format and args.  Load one or more objects from rows
 * using loadFunc.  Check the number of rows returned against the sqlQueryOpts
 * bit set. */
void *sqlQueryObjs(struct sqlConnection *conn, sqlLoadFunc loadFunc,
                   unsigned opts, char *queryFmt, ...)
/* Generate a query from format and args.  Load one or more objects from rows
 * using loadFunc.  Check the number of rows returned against the sqlQueryOpts
 * bit set.  Designed for use with autoSql, although load function must be
 * cast to sqlLoadFunc. */
{
struct slList *objs = NULL;
va_list args;
va_start(args, queryFmt);
objs = sqlVaQueryObjs(conn, loadFunc, opts, queryFmt, args);
va_end(args);
return objs;
}

int sqlSaveQuery(struct sqlConnection *conn, char *query, char *outPath, boolean isFa)
/* Execute query, save the resultset as a tab-separated file.
 * If isFa is true, than assume it is a two column fasta query and format accordingly.
 * Return count of rows in result set.  Abort on error. */
{
struct sqlResult *sr;
char **row;
char *sep="";
int c = 0;
int count = 0;
int numCols = 0;
FILE *f = mustOpen(outPath,"w");
sr = sqlGetResult(conn, query);
numCols = sqlCountColumns(sr);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sep="";
    if (isFa)
	sep = ">";
    for (c=0;c<numCols;++c)
	{
	fprintf(f,"%s%s",sep,row[c]);
	sep = "\t";
	if (isFa)
	    sep = "\n";
	}
    fprintf(f,"\n");	    
    ++count;
    }
sqlFreeResult(&sr);
carefulClose(&f);
return count;
}

char *sqlTempTableName(struct sqlConnection *conn, char *prefix)
/* Return a name for a temporary table. Name will start with
 * prefix.  This call doesn't actually  make table.  (So you should 
 * make table before next call to insure uniqueness.)  However the
 * table name encorperates the host, pid, and time, which helps insure
 * uniqueness between different processes at least.  FreeMem the result
 * when you are done. */
{
int i;
char tableName[PATH_LEN];
for (i=0; ;i++)
    {
    char *x = semiUniqName(prefix);
    safef(tableName, sizeof(tableName), "%s%d", x, i);
    if (!sqlTableExists(conn, tableName))
        break;
    }
return cloneString(tableName);
}

