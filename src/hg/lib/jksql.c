/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* jksql.c - Stuff to manage interface with SQL database. */

/*
 * Configuration:
 */


#include "common.h"
#include "portable.h"
#include "errabort.h"
#include <mysql.h>
#include "dlist.h"
#include "dystring.h"
#include "jksql.h"
#include "sqlNum.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: jksql.c,v 1.141 2010/06/03 05:14:39 kent Exp $";

/* flags controlling sql monitoring facility */
static unsigned monitorInited = FALSE;      /* initialized yet? */
static unsigned monitorFlags = 0;           /* flags indicating what is traced */
static long monitorEnterTime = 0;           /* time current tasked started */
static long long sqlTotalTime = 0;          /* total real milliseconds */
static long sqlTotalQueries = 0;            /* total number of queries */
static boolean monitorHandlerSet = FALSE;   /* is exit handler installed? */
static unsigned traceIndent = 0;            /* how much to indent */
static char *indentStr = "                                                       ";
static boolean sqlParanoid = FALSE;         /* extra squawking */

/* statistics */
static unsigned totalNumConnects = 0;
static unsigned maxNumConnections = 0;

struct sqlProfile
/* a configuration profile for connecting to a server */
{
    struct sqlProfile *next;
    char *name;         // name of profile
    char *host;         // host name for database server
    unsigned int port;  // port for database server
    char *socket;       // unix-domain socket path for database server
    char *user;         // database server user name
    char *password;     // database server password
    struct slName *dbs; // database associated with profile, can be NULL.
    };

struct sqlConnection
/* This is an item on a list of sql open connections. */
    {
    MYSQL *conn;		    /* Connection. */
    struct sqlProfile *profile;     /* profile, or NULL if not opened via a profile */
    struct dlNode *node;	    /* Pointer to list node. */
    struct dlList *resultList;	    /* Any open results. */
    boolean hasHardLock;	    /* TRUE if table has a non-advisory lock. */
    boolean inCache;                /* debugging flag to indicate it's in a cache */
    boolean isFree;                /* is this connection free for reuse; alway FALSE
                                    * unless managed by a cache */
    };

struct sqlResult
/* This is an item on a list of sql open results. */
    {
    MYSQL_RES *result;			/* Result. */
    struct dlNode *node;		/* Pointer to list node we're on. */
    struct sqlConnection *conn;		/* Pointer to connection. */
    long fetchTime;                     /* cummulative time taken by row fetches for this result */
    };

static struct dlList *sqlOpenConnections = NULL;
static unsigned sqlNumOpenConnections = 0;

char *defaultProfileName = "db";                  // name of default profile
static struct hash *profiles = NULL;              // profiles parsed from hg.conf, by name
static struct sqlProfile *defaultProfile = NULL;  // default profile, also in profiles list
static struct hash* dbToProfile = NULL;           // db to sqlProfile

static char *envOverride(char *envName, char *defaultVal)
/* look up envName in environment, if it exists and is non-empty, return its
 * value, otherwise return defaultVal */
{
char *val = getenv(envName);
if (isEmpty(val))
    return defaultVal;
else
    return val;
}

static struct sqlProfile *sqlProfileNew(char *profileName, char *host, unsigned int port,
					char *socket, char *user, char *password)
/* create a new profile object */
{
struct sqlProfile *sp;
AllocVar(sp);
sp->name = cloneString(profileName);
sp->host = cloneString(host);
sp->port = port;
sp->socket = cloneString(socket);
sp->user = cloneString(user);
sp->password = cloneString(password);
return sp;
}

static void sqlProfileAssocDb(struct sqlProfile *sp, char *db)
/* associate a db with a profile.  If it is already associated with this
 * profile, don't do anything.*/
{
struct sqlProfile *sp2 = hashFindVal(dbToProfile, db);
if ((sp2 != NULL) && (sp2 != sp))
    errAbort("databases %s already associated with profile %s, trying to associated it with %s",
             db, sp2->name, sp->name);
if (sp2 == NULL)
    {
    hashAdd(dbToProfile, db, sp);
    slSafeAddHead(&sp->dbs, slNameNew(db));
    }
}

static void sqlProfileCreate(char *profileName, char *host, unsigned int port,
			    char *socket, char *user, char *password)
/* create a profile and add to global data structures */
{
struct sqlProfile *sp = sqlProfileNew(profileName, host, port, socket, user, password);
hashAdd(profiles, sp->name, sp);
if (sameString(sp->name, defaultProfileName))
    defaultProfile = sp;  // save default
}

static void sqlProfileAddProfIf(char *profileName)
/* check if a config prefix is a profile, and if so, add a
 * sqlProfile object for it if doesn't already exist. */
{
char *host = cfgOption2(profileName, "host");
char *portstr = cfgOption2(profileName, "port");
char *socket = cfgOption2(profileName, "socket");
char *user = cfgOption2(profileName, "user");
char *password = cfgOption2(profileName, "password");
unsigned int port = 0;

if ((host != NULL) && (user != NULL) && (password != NULL) && (hashLookup(profiles, profileName) == NULL))
    {
    /* for the default profile, allow environment variable override */
    if (sameString(profileName, defaultProfileName))
        {
        host = envOverride("HGDB_HOST", host);
        portstr = envOverride("HGDB_PORT", portstr);
        socket = envOverride("HGDB_SOCKET", socket);
        user = envOverride("HGDB_USER", user);
        password = envOverride("HGDB_PASSWORD", password);
        }

    if (portstr != NULL)
	port = atoi(portstr);

    sqlProfileCreate(profileName, host, port, socket, user, password);
    }
}

static void sqlProfileAddProfs(struct slName *cnames)
/* load the profiles from list of config names */
{
struct slName *cname;
for (cname = cnames; cname != NULL; cname = cname->next)
    {
    char *dot1 = strchr(cname->name, '.'); // first dot in name
    if ((dot1 != NULL) && sameString(dot1, ".host"))
        {
        *dot1 = '\0';
        sqlProfileAddProfIf(cname->name);
        *dot1 = '.';
        }
    }
}

void sqlProfileAddDb(char *profileName, char *db)
/* add a mapping of db to profile.  If database is already associated with
 * this profile, it is ignored.  If it is associated with a different profile,
 * it is an error. */
{
struct sqlProfile *sp = hashFindVal(profiles, profileName);
if (sp == NULL)
    errAbort("can't find profile %s for database %s in hg.conf", profileName, db);
sqlProfileAssocDb(sp, db);
}

static void sqlProfileAddDbs(struct slName *cnames)
/* add mappings of db to profile from ${db}.${profile} entries.
 * would have liked to have automatically added ${profile}.db
 * entries, but backupcentral, etc, would map multiple profiles
 * to a databases, so this is done manually in hdb.c. */
{
struct slName *cname;
for (cname = cnames; cname != NULL; cname = cname->next)
    {
    char *dot1 = strchr(cname->name, '.'); // first dot in name
    if ((dot1 != NULL) && sameString(dot1, ".profile"))
        {
        char *profileName = cfgVal(cname->name);
        *dot1 = '\0';
        sqlProfileAddDb(profileName, cname->name);
        *dot1 = '.';
        }
    }
}

static void sqlProfileLoad(void)
/* load the profiles from config */
{
profiles = hashNew(8);
dbToProfile = hashNew(12);
struct slName *cnames = cfgNames();
sqlProfileAddProfs(cnames);
sqlProfileAddDbs(cnames);
slFreeList(&cnames);
}

static struct sqlProfile* sqlProfileFindByName(char *profileName, char *database)
/* find a profile by name, checking that database matches if found */
{
struct sqlProfile* sp = hashFindVal(profiles, profileName);
if (sp == NULL)
    return NULL;
#if UNUSED // FIXME: this breaks hgHeatMap, enable when logicalDb removed
if ((database != NULL) && (sp->dbs != NULL) && !slNameInList(sp->dbs, database))
    errAbort("attempt to obtain SQL profile %s for database %s, "
             "which is not associate with this database-specific profile",
             profileName, database);
#endif
return sp;
}

static struct sqlProfile* sqlProfileFindByDatabase(char *database)
/* find a profile using database as profile name, return the default if not
 * found */
{
struct sqlProfile *sp = hashFindVal(dbToProfile, database);
if (sp == NULL)
    sp = defaultProfile;
return sp;
}

static struct sqlProfile* sqlProfileGet(char *profileName, char *database)
/* lookup a profile using the profile resolution algorithm:
 *  - If a profile is specified:
 *     - search hg.conf for the profile, if found:
 *       - if database is specified, then either
 *           - the profile should not specify a database
 *           - the database must match the database in the profile
 *  - If a profile is not specified:
 *     - search hg.conf for a profile with the same name as the database
 *     - if there is no profile named the same as the database, use
 *       the default profile of "db"
 * return NULL if not found.
 */
{
assert((profileName != NULL) || (database != NULL));
if (profiles == NULL)
    sqlProfileLoad();

if (profileName != NULL)
    return sqlProfileFindByName(profileName, database);
else
    return sqlProfileFindByDatabase(database);
}

static struct sqlProfile* sqlProfileMustGet(char *profileName, char *database)
/* lookup a profile using the profile resolution algorithm or die trying */
{
struct sqlProfile* sp = sqlProfileGet(profileName, database);
if (sp == NULL)
    {
    if (profileName == NULL)
        errAbort("can't find database %s in hg.conf, should have a default named \"db\"",
                 database);
    else if (database == NULL)
        errAbort("can't find profile %s in hg.conf", profileName);
    else
        errAbort("can't find profile %s for database %s in hg.conf", profileName, database);
    }
return sp;
}

struct slName* sqlProfileGetNames()
/* Get a list of all profile names. slFreeList result when done */
{
if (profiles == NULL)
    sqlProfileLoad();
struct slName *names = NULL;
struct hashCookie cookie = hashFirst(profiles);
struct hashEl* hel;
while ((hel = hashNext(&cookie)) != NULL)
    slAddHead(&names, slNameNew(hel->name));
return names;
}

static void replaceStr(char **str, char *val)
/* free str and replace with clone new value */
{
freeMem(*str);
*str = cloneString(val);
}

void sqlProfileConfig(char *profileName, char *host, unsigned int port,
			char *socket, char *user, char *password)
/* Set configuration for the profile.  This overrides an existing profile in
 * hg.conf or defines a new one.  Results are unpredictable if a connect cache
 * has been established for this profile. */
{
struct sqlProfile* sp = sqlProfileGet(profileName, NULL);
if (sp == NULL)
    return  sqlProfileCreate(profileName, host, port, socket, user, password);
replaceStr(&sp->host, host);
replaceStr(&sp->socket, socket);
sp->port = port;
replaceStr(&sp->user, user);
replaceStr(&sp->password, password);
}

void sqlProfileConfigDefault(char *host, unsigned int port, char *socket,
				char *user, char *password)
/* Set configuration for the default profile.  This overrides an existing
 * profile in hg.conf or defines a new one.  Results are unpredictable if a
 * connect cache has been established for this profile. */
{
sqlProfileConfig(defaultProfileName, host, port, socket, user, password);
}

static void monitorInit(void)
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

static void monitorEnter(void)
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

static long monitorLeave(void)
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

static char *scConnDb(struct sqlConnection *sc)
/* Return sc->conn->db, unless it is NULL -- if NULL, return a string for
 * fprint'd messages. */
{
return (sc->conn->db ? sc->conn->db : "db=?");
}

static void monitorPrintInfo(struct sqlConnection *sc, char *name)
/* print a monitor message, with connection id and databases. */
{
fprintf(stderr, "%.*s%s %ld %s\n", traceIndent, indentStr, name,
        sc->conn->thread_id, scConnDb(sc));
fflush(stderr);
}

static void monitorPrint(struct sqlConnection *sc, char *name,
                         char *format, ...)
/* print a monitor message, with connection id, databases, and
 * printf style message.*/
{
va_list args;
fprintf(stderr, "%.*s%s %ld %s ", traceIndent, indentStr, name,
        sc->conn->thread_id, scConnDb(sc));
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
fflush(stderr);
}

static void monitorPrintTime(void)
/* print total time */
{
/* only print if not explictly disabled */
if (monitorFlags & JKSQL_PROF)
    {
    fprintf(stderr, "%.*sSQL_TOTAL_TIME %0.3fs\n", traceIndent, indentStr,
            ((double)sqlTotalTime)/1000.0);
    fprintf(stderr, "%.*sSQL_TOTAL_QUERIES %ld\n", traceIndent, indentStr,
            sqlTotalQueries);
    fflush(stderr);
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

void sqlMonitorDisable(void)
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
    if (monitorFlags & JKSQL_TRACE)
        monitorPrint(res->conn, "SQL_FETCH", "%0.3fs", ((double) res->fetchTime)/1000.0);
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
    if (sc->inCache)
        errAbort("sqlDisconnect called on connection associated with a cache");
    assert(!sc->isFree);
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
    sqlNumOpenConnections--;
    }
}

char* sqlGetDatabase(struct sqlConnection *sc)
/* Get the database associated with an connection. Warning: return may be NULL! */
{
assert(!sc->isFree);
return sc->conn->db;
}

char* sqlGetHost(struct sqlConnection *sc)
/* Get the host associated with an connection. */
{
assert(!sc->isFree);
return sc->conn->host;
}

struct slName *sqlGetAllDatabase(struct sqlConnection *sc)
/* Get a list of all database on the server */
{
struct sqlResult *sr = sqlGetResult(sc, "show databases");
char **row;
struct slName *databases = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!startsWith("mysql", row[0]))  /* Avoid internal databases. */
        slSafeAddHead(&databases, slNameNew(row[0]));
    }
sqlFreeResult(&sr);
return databases;
}

struct slName *sqlListTables(struct sqlConnection *conn)
/* Return list of tables in database associated with conn. */
{
struct sqlResult *sr;
char **row;
struct slName *list = NULL, *el;
char *cfgName = "showTableCache";
char *tableList = cfgOption(cfgName);

if (tableList != NULL)
    {
    // mysql does not cache "show tables", so use a cached run of show tables which is stored in the showTableCache table.
    // See redmine 3780 for details.
    if(sqlTableExists(conn, tableList))
        {
        char query[256];
        safef(query, sizeof(query), "select * from %s order by name desc", tableList);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            {
            el = slNameNew(row[0]);
            slAddHead(&list, el);
            }
        }
    else
        {
        errAbort("%s option is misconfigured in hg.conf: table '%s' does not exist", cfgName, tableList);
        }
    }
else
    {
    sr = sqlGetResult(conn, "show tables");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        el = slNameNew(row[0]);
        slAddHead(&list, el);
        }
    slReverse(&list);
    }
sqlFreeResult(&sr);
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

void sqlAddDatabaseFields(char *database, struct hash *hash)
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

struct hash *sqlAllFields(void)
/* Get hash of all fields in database.table.field format.  */
{
struct hash *fullHash = hashNew(18);
struct hash *dbHash = sqlHashOfDatabases();
struct hashEl *dbList, *db;
dbList = hashElListHash(dbHash);
for (db = dbList; db != NULL; db = db->next)
    sqlAddDatabaseFields(db->name, fullHash);
slFreeList(&dbList);
hashFree(&dbHash);
return fullHash;
}


void sqlCleanupAll(void)
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
        conn->inCache = FALSE; // really should be cleaning up caches too
        conn->isFree = FALSE;
	sqlDisconnect(&conn);
	}
    freeDlList(&sqlOpenConnections);
    }
}

static void sqlInitTracking(void)
/* Initialize tracking and freeing of resources. */
{
if (sqlOpenConnections == NULL)
    {
    sqlOpenConnections = newDlList();
    atexit(sqlCleanupAll);
    }
}

static struct sqlConnection *sqlConnRemote(char *host, unsigned int port, char *socket,
					   char *user, char *password,
                                           char *database, boolean abort)
/* Connect to database somewhere as somebody. Database maybe NULL to just
 * connect to the server.  If abort is set display error message and abort on
 * error. This is the core function that connects to a MySQL server. */
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
	port,	/* port */
	socket,	/* socket */
	0)	/* flags */  == NULL)
    {
    monitorLeave();
    if (abort)
	errAbort("Couldn't connect to database %s on %s as %s.\n%s",
	    database, host, user, mysql_error(conn));
    else if (sqlParanoid)
	fprintf(stderr, "ASH: Couldn't connect to database %s on %s as %s.  "
		"mysql: %s  pid=%ld\n",
		database, host, user, mysql_error(conn), (long)getpid());
    return NULL;
    }

/* Make sure the db is correct in the connect, think usually happens if there
 * is a mismatch between MySQL library and code.  If this happens, please
 * figure out what is going on.  Contact markd if you need help. */
if (((conn->db != NULL) && !sameString(database, conn->db))
   || ((conn->db == NULL) && (database != NULL)))
   errAbort("apparent mismatch between mysql.h used to compile jksql.c and libmysqlclient");

if (monitorFlags & JKSQL_TRACE)
    monitorPrint(sc, "SQL_CONNECT", "%s %s", host, user);

deltaTime = monitorLeave();
if (monitorFlags & JKSQL_TRACE)
    monitorPrint(sc, "SQL_TIME", "%0.3fs", ((double)deltaTime)/1000.0);
sqlNumOpenConnections++;
if (sqlNumOpenConnections > maxNumConnections)
    maxNumConnections = sqlNumOpenConnections;
totalNumConnects++;
return sc;
}

struct sqlConnection *sqlConnectRemote(char *host, char *user, char *password,
                                       char *database)
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server. Abort on error. */
{
return sqlConnRemote(host, 0, NULL, user, password, database, TRUE);
}

struct sqlConnection *sqlMayConnectRemote(char *host, char *user, char *password,
                                          char *database)
/* Connect to database somewhere as somebody. Database maybe NULL to
 * just connect to the server.  Return NULL can't connect */
{
return sqlConnRemote(host, 0, NULL, user, password, database, FALSE);
}

static struct sqlConnection *sqlConnProfile(struct sqlProfile* sp, char *database, boolean abort)
/* Connect to database using the profile.  Database maybe NULL to connect to
 * the server. Optionally abort on failure. */
{
struct sqlConnection *conn = sqlConnRemote(sp->host, sp->port, sp->socket, sp->user, sp->password, database, abort);
if (conn != NULL)
    conn->profile = sp;  // remember profile, mainly for debugging
return conn;
}

struct sqlConnection *sqlMayConnect(char *database)
/* Connect to database on default host as default user.
 * Return NULL (don't abort) on failure. */
{
return sqlConnProfile(sqlProfileMustGet(NULL, database), database, FALSE);
}

struct sqlConnection *sqlConnect(char *database)
/* Connect to database on default host as default user. */
{
return sqlConnProfile(sqlProfileMustGet(NULL, database), database, TRUE);
}

struct sqlConnection *sqlConnectProfile(char *profileName, char *database)
/* Connect to profile or database using the specified profile.  Can specify
 * profileName, database, or both. The profile is the prefix to the host,
 * user, and password variables in .hg.conf.  For the default profile of "db",
 * the environment variables HGDB_HOST, HGDB_USER, and HGDB_PASSWORD can
 * override.
 */
{
struct sqlProfile* sp = sqlProfileMustGet(profileName, database);
return sqlConnRemote(sp->host, sp->port, sp->socket, sp->user, sp->password, database, TRUE);
}

struct sqlConnection *sqlMayConnectProfile(char *profileName, char *database)
/* Connect to profile or database using the specified profile. Can specify
 * profileName, database, or both. The profile is the prefix to the host,
 * user, and password variables in .hg.conf.  For the default profile of "db",
 * the environment variables HGDB_HOST, HGDB_USER, and HGDB_PASSWORD can
 * override.  Return NULL if connection fails.
 */
{
struct sqlProfile* sp = sqlProfileGet(profileName, database);
return sqlConnRemote(sp->host, sp->port, sp->socket, sp->user, sp->password, database, FALSE);
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
/* Returns NULL if result was empty and getter==mysql_use_result.
 * Otherwise returns a structure that you can do sqlRow() on.
 * Watch out for subtle differences between mysql_store_result and mysql_use_result.
 * We seem to be only using mysql_use_result these days,
 * but mysql_store_result has left a big footprint in the code/comments.
 * In particular, mysql_store_result can return NULL indicating an empty resultset.
 * But mysql_use_result cannot do that. Instead NULL return means error
 * and the user must call next_row to see if there's anything in the resultset.
 */
{
assert(!sc->isFree);
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
        res->fetchTime = 0L;
        }
    }
deltaTime = monitorLeave();
if (monitorFlags & JKSQL_TRACE)
    monitorPrint(sc, "SQL_TIME", "%0.3fs", ((double)deltaTime)/1000.0);
return res;
}

void sqlRenameTable(struct sqlConnection *sc, char *table1, char *table2)
/* Rename table1 to table2 */
{
char query[256];
safef(query, sizeof(query), "rename table %s to %s", table1, table2);
sqlUpdate(sc, query);
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

void sqlCopyTable(struct sqlConnection *sc, char *table1, char *table2)
/* Copy table1 to table2 */
{
char query[256];

if (table1 == NULL || table2 == NULL)
    return;
safef(query, sizeof(query), "create table %s like %s", table2, table1);
sqlUpdate(sc, query);
safef(query, sizeof(query), "insert into %s select * from  %s", table2, table1);
sqlUpdate(sc, query);
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
        break;
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

safef(query, sizeof(query), "SELECT 1 FROM %s LIMIT 0", table);
if ((sr = sqlUseOrStore(sc,query,mysql_use_result, FALSE)) == NULL)
    return FALSE;
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
    sr->fetchTime += monitorLeave();
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



struct sqlResult *sqlGetResultExt(struct sqlConnection *sc, char *query, unsigned int *errorNo, char **error)
/* Returns NULL if it had an error.
 * Otherwise returns a structure that you can do sqlRow() on.
 * If there was an error, *errorNo will be set to the mysql error number,
 * and *error will be set to the mysql error string, which MUST NOT be freed. */
{
struct sqlResult *sr = sqlUseOrStore(sc, query, mysql_use_result, FALSE);
if (sr == NULL)
    {
    MYSQL *conn = sc->conn;
    if (errorNo)
    	*errorNo=mysql_errno(conn);
    if (error)
    	*error=(char *)mysql_error(conn);
    }
else
    {
    if (errorNo)
    	*errorNo=0;
    if (error)
    	*error=NULL;
    }
return sr;
}


struct sqlResult *sqlGetResult(struct sqlConnection *sc, char *query)
/* (Returns NULL if result was empty. :
 *     old info, only applies with mysql_store_result not mysql_use_result)
 * Otherwise returns a structure that you can do sqlRow() on. */
{
return sqlUseOrStore(sc,query,mysql_use_result, TRUE);
}

struct sqlResult *sqlMustGetResult(struct sqlConnection *sc, char *query)
/* Query database.
 * old comment: If result empty squawk and die.
 *    This only applied back when sqlGetResult was using mysql_store_result.
 * These days, with mysql_use_result, we cannot know ahead of time
 * if there are results, we can only know by actually trying to fetch a row.
 * At then how would we put it back?  So in fact right now sqlMustGetResult
 * is no different than sqlGetResult.  */
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

void sqlWarnings(struct sqlConnection *conn, int numberOfWarnings)
/* Show the number of warnings requested. New feature in mysql5. */
{
struct sqlResult *sr;
char **row;
char query[256];
struct dyString *dy = dyStringNew(0);
safef(query,sizeof(query),"show warnings limit 0, %d", numberOfWarnings);
sr = sqlGetResult(conn, query);
dyStringPrintf(dy, "Level Code Message\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    dyStringPrintf(dy, "%s %s %s\n", row[0], row[1], row[2]);
    }
sqlFreeResult(&sr);
warn("%s", dy->string);
dyStringFree(&dy);
}

void sqlLoadTabFile(struct sqlConnection *conn, char *path, char *table,
                    unsigned options)
/* Load a tab-seperated file into a database table, checking for errors.
 * Options are the SQL_TAB_* bit set. SQL_TAB_FILE_ON_SERVER is ignored if
 * sqlIsRemote() returns true. */
{
assert(!conn->isFree);
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
if ((options & SQL_TAB_FILE_ON_SERVER) && !sqlIsRemote(conn))
    {
    /* tab file on server requiries full path */
    strcpy(tabPath, "");
    if (path[0] != '/')
        {
        if (getcwd(tabPath, sizeof(tabPath)) == NULL)
	    errAbort("sqlLoadTableFile: getcwd failed");
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
    if (numWarnings > 0)
	{
	sqlWarnings(conn, 10);  /* show the first 10 warnings */
	}
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

int sqlRowCount(struct sqlConnection *conn, char *queryTblAndCondition)
/* Return count of rows that match condition. The queryTblAndCondition
 * should contain everying after "select count(*) FROM " */
{
char query[256];
safef(query, sizeof(query), "select count(*) from %s",queryTblAndCondition);
return sqlQuickNum(conn, query);
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

struct slName *sqlFieldNames(struct sqlConnection *conn, char *table)
/* Returns field names from a table. */
{
struct slName *list = NULL;
char query[256];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slNameAddHead(&list, row[0]);
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

unsigned int sqlLastAutoId(struct sqlConnection *conn)
/* Return last automatically incremented id inserted into database. */
{
assert(!conn->isFree);
unsigned id;
monitorEnter();
id = mysql_insert_id(conn->conn);
monitorLeave();
return id;
}

/* Stuff to manage and caches of open connections on a database.  Typically
 * you only need 3.  MySQL takes about 2 milliseconds on a local host to open
 * a connection.  On a remote host it can be more and this caching is probably
 * actually necessary. However, much code has been written assuming caching,
 * so it is probably now necessary.
 */

enum {sqlConnCacheMax = 16};

struct sqlConnCache
{
    /* the following are NULL unless explicitly specified */
    char *host;					/* Host machine of database. */
    char *user;					/* Database user name */
    char *password;				/* Password. */
    struct sqlProfile *profile;                 /* restrict to this profile */
    /* contents of cache */
    int entryCnt;                               /* # open connections. */
    struct sqlConnCacheEntry *entries;          /* entries in the cache */
};

struct sqlConnCacheEntry
/* an entry in the cache */
{
    struct sqlConnCacheEntry *next;
    struct sqlProfile *profile;      /* profile for connection, can be NULL if host is explicit */
    struct sqlConnection *conn;      /* connection */
    boolean inUse;                   /* is this in use? */
};

struct sqlConnCache *sqlConnCacheNewRemote(char *host, char *user,
                                           char *password)
/* Set up a cache on a remote database. */
{
struct sqlConnCache *cache;
AllocVar(cache);
cache->host = cloneString(host);
cache->user = cloneString(user);
cache->password = cloneString(password);
return cache;
}

struct sqlConnCache *sqlConnCacheNew()
/* Return a new connection cache. */
{
struct sqlConnCache *cache;
AllocVar(cache);
return cache;
}

struct sqlConnCache *sqlConnCacheNewProfile(char *profileName)
/* Return a new connection cache associated with the particular profile. */
{
struct sqlConnCache *cache = sqlConnCacheNew();
cache->profile = sqlProfileMustGet(profileName, NULL);
return cache;
}

void sqlConnCacheFree(struct sqlConnCache **pCache)
/* Dispose of a connection cache. */
{
struct sqlConnCache *cache;
if ((cache = *pCache) != NULL)
    {
    struct sqlConnCacheEntry *scce;
    for (scce = cache->entries; scce != NULL; scce = scce->next)
        {
        scce->conn->inCache = FALSE;
        scce->conn->isFree = FALSE;
	sqlDisconnect(&scce->conn);
        }
    slFreeList(&cache->entries);
    freeMem(cache->host);
    freeMem(cache->user);
    freeMem(cache->password);
    freez(pCache);
    }
}

static struct sqlConnCacheEntry *sqlConnCacheAdd(struct sqlConnCache *cache,
                                                 struct sqlProfile *profile,
                                                 struct sqlConnection *conn)
/* create and add a new cache entry */
{
struct sqlConnCacheEntry *scce;
AllocVar(scce);
scce->profile = profile;
scce->conn = conn;
conn->inCache = TRUE;
conn->isFree = TRUE;
slAddHead(&cache->entries, scce);
cache->entryCnt++;
return scce;
}

static boolean sqlConnCacheEntryDbMatch(struct sqlConnCacheEntry *scce,
                                        char *database)
/* does a database match the one in the connection cache */
{
return ((database == NULL) && (scce->conn->conn->db == NULL))
    || sameString(database, scce->conn->conn->db);
}

static boolean sqlConnCacheEntrySetDb(struct sqlConnCacheEntry *scce,
                                      char *database,
                                      boolean abort)
/* set the connect cache and connect to the specified database */
{
if (mysql_select_db(scce->conn->conn, database) != 0)
    {
    if (abort) 
        errAbort("Couldn't set connection database to %s\n%s",
                 database, mysql_error(scce->conn->conn));
    else
        return FALSE;
    }
return TRUE;
}

static struct sqlConnCacheEntry *sqlConnCacheFindFree(struct sqlConnCache *cache,
                                                      struct sqlProfile *profile,
                                                      char *database,
                                                      boolean matchDatabase)
/* find a free entry associated with profile and database. Return NULL if no
 * entries are available.  Will attempt to match database if requested, this
 * includes connections to no database (database==NULL). */
{
struct sqlConnCacheEntry *scce;
for (scce = cache->entries; scce != NULL; scce = scce->next)
    {
    if (!scce->inUse && (profile == scce->profile)
        && ((!matchDatabase) || sqlConnCacheEntryDbMatch(scce, database)))
        {
        return scce;
        }
    }
return NULL;
}

static struct sqlConnCacheEntry *sqlConnCacheAddNew(struct sqlConnCache *cache,
                                                    struct sqlProfile *profile,
                                                    char *database,
                                                    boolean abort)
/* create and add a new connect to the cache */
{
struct sqlConnection *conn;
if (cache->entryCnt >= sqlConnCacheMax)
    errAbort("Too many open sqlConnections for cache");
if (cache->host != NULL)
    conn = sqlConnRemote(cache->host, 0, NULL, cache->user,
                         cache->password, database, abort);
else
    conn = sqlConnProfile(profile, database, abort);
if (conn != NULL)
    return sqlConnCacheAdd(cache, profile, conn);
else
    {
    assert(!abort);
    return NULL;
    }
}

static struct sqlConnection *sqlConnCacheDoAlloc(struct sqlConnCache *cache,
                                                 char *profileName,
                                                 char *database,
                                                 boolean abort)
/* Allocate a cached connection. errAbort if too many open connections.
 * errAbort if abort and connection fails. */
{
// obtain profile
struct sqlProfile *profile = NULL;
if ((cache->host != NULL) && (profileName != NULL))
    errAbort("can't specify profileName (%s) when sqlConnCache is create with a specific host (%s)",
             profileName, cache->host);
if ((profileName != NULL) && (cache->profile != NULL)
    && !sameString(profileName, cache->profile->name))
    errAbort("profile name %s doesn't match profile associated with sqlConnCache %s",
             profileName, cache->profile->name);
if (cache->profile != NULL)
    profile = cache->profile;
else
    profile = sqlProfileMustGet(profileName, database);

// try getting an entry, first trying to find one for this database, then
// look for any database, then add a new one
struct sqlConnCacheEntry *scce
    = sqlConnCacheFindFree(cache, profile, database, TRUE);
if (scce == NULL)
    {
    scce = sqlConnCacheFindFree(cache, profile, database, FALSE);
    if (scce != NULL)
        {
        if (!sqlConnCacheEntrySetDb(scce, database, abort))
            scce = NULL;  // got error with no abort
        }
    else
        scce = sqlConnCacheAddNew(cache, profile, database, abort);
    }
if (scce != NULL)
    {
    assert(scce->conn->isFree);
    scce->inUse = TRUE;
    scce->conn->isFree = FALSE;
    return scce->conn;
    }
else
    return NULL;
}

struct sqlConnection *sqlConnCacheMayAlloc(struct sqlConnCache *cache,
                                           char *database)
/* Allocate a cached connection. errAbort if too many open connections,
 * return NULL if can't connect to server. */
{
return sqlConnCacheDoAlloc(cache, NULL, database, FALSE);
}

struct sqlConnection *sqlConnCacheAlloc(struct sqlConnCache *cache,
                                        char *database)
/* Allocate a cached connection. */
{
return sqlConnCacheDoAlloc(cache, NULL, database, TRUE);
}

struct sqlConnection *sqlConnCacheProfileAlloc(struct sqlConnCache *cache,
                                               char *profileName,
                                               char *database)
/* Allocate a cached connection given a profile and/or database. */
{
return sqlConnCacheDoAlloc(cache, profileName, database, TRUE);
}

struct sqlConnection *sqlConnCacheProfileAllocMaybe(struct sqlConnCache *cache,
                                                    char *profileName,
                                                    char *database)
/* Allocate a cached connection given a profile and/or database. Return NULL
 * if the database doesn't exist.  */
{
return sqlConnCacheDoAlloc(cache, profileName, database, FALSE);
}

void sqlConnCacheDealloc(struct sqlConnCache *cache, struct sqlConnection **pConn)
/* Free up a cached connection. */
{
struct sqlConnection *conn = *pConn;
if (conn != NULL)
    {
    if (!conn->inCache)
        errAbort("sqlConnCacheDealloc called on connection that is not associated with a cache");
    assert(!conn->isFree);
    conn->isFree = TRUE;
    struct sqlConnCacheEntry *scce;
    for (scce = cache->entries; (scce != NULL) && (scce->conn != conn); scce = scce->next)
        continue;
    if (scce ==  NULL)
        errAbort("sqlConnCacheDealloc called on cache that doesn't contain "
                 "the given connection");
    scce->inUse = FALSE;
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

static void addProfileDatabases(char *profileName, struct hash *databases)
/* find databases on a profile and add to hash */
{
struct sqlConnection *sc = sqlMayConnectProfile(profileName, NULL);
if (sc != NULL)
    {
    struct slName *db, *dbs = sqlGetAllDatabase(sc);
    for (db = dbs; db != NULL; db = db->next)
        hashAdd(databases, db->name, NULL);
    sqlDisconnect(&sc);
    slFreeList(&dbs);
    }
}

struct hash *sqlHashOfDatabases(void)
/* Get hash table with names of all databases that are online. */
{
if (profiles == NULL)
    sqlProfileLoad();
struct hash *databases = newHash(8);
// add databases found using default profile
addProfileDatabases(defaultProfileName, databases);

// add other databases explicitly associated with other profiles
struct hashCookie cookie = hashFirst(dbToProfile);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    hashAdd(databases, ((struct sqlProfile*)hel->val)->name, NULL);
return databases;
}

struct slName *sqlListOfDatabases(void)
/* Get list of all databases that are online. */
{
/* build hash and convert to names list to avoid duplicates due to visiting
 * multiple profiles to the same server */
struct hash *dbHash = sqlHashOfDatabases();
struct hashCookie cookie = hashFirst(dbHash);
struct hashEl *hel;
struct slName *dbs = NULL;
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&dbs, slNameNew(hel->name));
hashFree(&dbHash);
slSort(&dbs, slNameCmp);
return dbs;
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
/* Ask mktime to determine whether Daylight Savings Time is in effect for
 * the given time: */
tm->tm_isdst = -1;
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

time_t sqlTableUpdateTime(struct sqlConnection *conn, char *table)
/* Get last update time for table. */
{
char *date = sqlTableUpdate(conn, table);
time_t time = sqlDateToUnixTime(date);
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

void sqlSetParanoid(boolean beParanoid)
/* If set to TRUE, will make more diagnostic stderr messages. */
{
sqlParanoid = beParanoid;
}

boolean sqlIsRemote(struct sqlConnection *conn)
/* test if the conn appears to be to a remote system.
 * Current only tests for a TCP/IP connection */
{
return (conn->conn->unix_socket == NULL);
}

static void sqlDumpProfile(struct sqlProfile *sp, FILE *fh)
/* dump one db profile */
{
fprintf(fh, "profile: %s host: %s user: %s dbs:", sp->name, sp->host, sp->user);
struct slName *db;
for (db = sp->dbs; db != NULL; db = db->next)
    fprintf(fh, " %s", db->name);
fputc('\n', fh);
}

static void sqlDumpConnection(struct sqlConnection *conn, FILE *fh)
/* dump an sql connection for debugging */
{
fprintf(fh, "conn: profile: %s host: %s db: %s results: %d",
        conn->profile->name, conn->conn->host, conn->conn->db, dlCount(conn->resultList));
if (conn->hasHardLock)
    fputs(" hardLocked", fh);
if (conn->inCache)
    fputs(" cached", fh);
if (conn->isFree)
    fputs(" free", fh);
fputc('\n', fh);
}

void sqlDump(FILE *fh)
/* dump internal info about SQL configuration for debugging purposes */
{
static char *dashes = "--------------------------------------------------------";
fprintf(fh, "%s\n", dashes);
fprintf(fh, "defaultProfile=%s\n", (defaultProfile != NULL) ? defaultProfile->name : "NULL");
struct hashCookie cookie = hashFirst(profiles);
struct hashEl *hel;
while((hel = hashNext(&cookie)) != NULL)
    sqlDumpProfile(hel->val, fh);

cookie = hashFirst(dbToProfile);
while((hel = hashNext(&cookie)) != NULL)
    fprintf(fh, "db: %s profile: %s\n", hel->name, ((struct sqlProfile*)hel->val)->name);

struct dlNode *connNode;
for (connNode = sqlOpenConnections->head; !dlEnd(connNode); connNode = connNode->next)
    sqlDumpConnection(connNode->val, fh);
fprintf(fh, "%s\n", dashes);
}

void sqlPrintStats(FILE *fh)
/* print statistic about the number of connections and other options done by
 * this process. */
{
fprintf(fh, "sqlStats: connects: %d maxOpen: %d\n", totalNumConnects, maxNumConnections);
}
    
