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
#include "jksql.h"
#include "hgConfig.h"

struct sqlConnection
/* This is an item on a list of sql open connections. */
    {
    MYSQL *conn;			    /* Connection. */
    struct dlNode *node;		    /* Pointer to list node. */
    struct dlList *resultList;		    /* Any open results. */
    };

struct sqlResult
/* This is an item on a list of sql open results. */
    {
    MYSQL_RES *result;			/* Result. */
    struct dlNode *node;		/* Pointer to list node we're on. */
    struct sqlConnection *conn;		/* Pointer to connection. */
    };

static struct dlList *sqlOpenConnections;

static char* getCfgValue(char* envName, char* cfgName)
/* get a configuration value, from either the environment or the cfg file,
 * with the env take precedence.
 */
{
char *val = getenv(envName);
if (val == NULL)
    val = cfgOption(cfgName);
return val;
}

void sqlFreeResult(struct sqlResult **pRes)
/* Free up a result. */
{
struct sqlResult *res = *pRes;
if (res != NULL)
    {
    if (res->result != NULL)
	mysql_free_result(res->result);
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
	mysql_close(conn);
	}
    if (node != NULL)
	{
	dlRemove(node);
	freeMem(node);
	}
    freez(pSc);
    }
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

struct sqlConnection *sqlConnectRemote(char *host, 
	char *user, char *password, char *database)
/* Connect to database somewhere as somebody. */
{
struct sqlConnection *sc;
MYSQL *conn;

sqlInitTracking();

AllocVar(sc);
sc->resultList = newDlList();
sc->node = dlAddValTail(sqlOpenConnections, sc);

if ((sc->conn = conn = mysql_init(NULL)) == NULL)
    errAbort("Couldn't connect to mySQL.");
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
    errAbort("Couldn't connect to database %s on %s as %s.\n%s", 
	database, host, user, mysql_error(conn));
    }
return sc;
}

struct sqlConnection *sqlConnect(char *database)
/* Connect to database on default host as default user. */
{
char* host = getCfgValue("HGDB_HOST", "db.host");
char* user = getCfgValue("HGDB_USER", "db.user");
char* password = getCfgValue("HGDB_PASSWORD", "db.password");

if(host == 0 || user == 0 || password == 0)
	errAbort("Could not read hostname, user, or password to the database from configuration file.");
	
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
if (mysql_real_query(conn, query, strlen(query)) != 0)
    {
    if (abort)
	sqlAbort(sc, "Can't start query:\n");
    return NULL;
    }
else
    {
    struct sqlResult *res;
    MYSQL_RES *resSet;
    if ((resSet = getter(conn)) == NULL)
	{
	if (mysql_errno(conn) != 0)
	    {
	    sqlAbort(sc, "Can't use query:\n%s", query);
	    }
	return NULL;
	}
    AllocVar(res);
    res->conn = sc;
    res->result = resSet;
    res->node = dlAddValTail(sc->resultList, res);
    return res;
    }
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

boolean sqlRemakeTable(struct sqlConnection *sc, char *table, char *create)
/* Drop table if it exists, and recreate it. */
{
if (sqlTableExists(sc, table))
    {
    char query[256];
    sprintf(query, "drop table %s", table);
    sqlUpdate(sc, query);
    }
sqlUpdate(sc, create);
}

boolean sqlTableExists(struct sqlConnection *sc, char *table)
/* Return TRUE if a table exists. */
{
char query[256];
struct sqlResult *sr;

sprintf(query, "select count(*) from %s", table);
if ((sr = sqlUseOrStore(sc,query,mysql_use_result, FALSE)) == NULL)
    return FALSE;
sqlNextRow(sr);	/* Just discard. */
sqlFreeResult(&sr);
return TRUE;
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
char** row = NULL;
if (sr != NULL)
    {
    row = mysql_fetch_row(sr->result);
    if (mysql_errno(sr->conn->conn) != 0)
        {
        sqlAbort(sr->conn, "nextRow failed");
        }
    }
return row;
}

/* repeated calls to this function returns the names of the fields 
 * the given result */
char* sqlFieldName(struct sqlResult *sr)
{
MYSQL_FIELD *field;

field = mysql_fetch_field(sr->result);
if(field == 0)
	return 0;

return field->name;
}

int sqlCountColumns(struct sqlResult *sr)
/* Count the number of columns in result. */
{
if(sr != 0)
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
if (row != NULL)
    {
    strncpy(buf, row[0], bufSize);
    ret = buf;
    }
sqlFreeResult(&sr);
return ret;
}

int sqlQuickNum(struct sqlConnection *conn, char *query)
/* Get numerical result from simple query */
{
struct sqlResult *sr;
char **row;
int ret = 0;

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    ret = sqlSigned(row[0]);
    }
sqlFreeResult(&sr);
return ret;
}

int sqlTableSize(struct sqlConnection *conn, char *table)
/* Find number of rows in table. */
{
char query[128];
sprintf(query, "select count(*) from %s", table);
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
sprintf(query, "describe %s", table);
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
return mysql_insert_id(conn->conn);
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

struct sqlConnection *sqlAllocConnection(struct sqlConnCache *cache)
/* Allocate a cached connection. */
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
cache->connArray[connAlloced] = sqlConnectRemote(cache->host, 
	cache->user, cache->password, cache->database);
connUsed[connAlloced] = TRUE;
++cache->connAlloced;
return cache->connArray[connAlloced];
}

void sqlFreeConnection(struct sqlConnCache *cache, struct sqlConnection **pConn)
/* Free up a cached connection. */
{
struct sqlConnection *conn;
int connAlloced = cache->connAlloced;
struct sqlConnection **connArray = cache->connArray;

if ((conn = *pConn) != NULL)
    {
    int ix;
    for (ix = 0; ix < connAlloced; ++ix)
	{
	if (connArray[ix] == conn)
	    {
	    cache->connUsed[ix] = FALSE;
	    break;
	    }
	}
    *pConn = NULL;
    }
}


char *sqlEscapeString(const char* from)
{
int size = (strlen(from)*2) +1;
char *to = needMem(size * sizeof(char));
mysql_escape_string(to, from, strlen(from));
return to; 
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
    hashAdd(hash, row[0], NULL);
    }
sqlDisconnect(&conn);
return hash;
}


