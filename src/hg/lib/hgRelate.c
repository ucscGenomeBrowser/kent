/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* hgRelate - Especially relational parts of browser data base. */
#include "common.h"
#include "portable.h"
#include "localmem.h"
#include "dystring.h"
#include "hash.h"
#include "fa.h"
#include "jksql.h"
#include "hgRelate.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgRelate.c,v 1.11 2003/06/11 03:24:47 kent Exp $";

static char extFileCreate[] =
/* This keeps track of external files and directories. */
"create table extFile ("
  "id int unsigned not null primary key,"  /* Unique ID across all tables. */
  "name varchar(64) not null,"	  /* Symbolic name of file.  */
  "path varchar(255) not null,"   /* Full path. Ends in '/' if a dir. */
  "size bigint unsigned not null,"           /* Size of file (checked) */
                   /* Extra indices. */
  "index (name))";

static char historyCreate[] =	
/* This contains a row for each update made to database.
 * (The idea is that this is just updated in batch.)
 * It keeps track of which id global ids are used
 * as well as providing a record of updates. */
"create table history ("
  "ix int not null auto_increment primary key,"  /* Update number. */
  "startId int unsigned not null,"              /* Start this session's ids. */
  "endId int unsigned not null,"                /* First id for next session. */
  "who varchar(255) not null,"         /* User who updated. */
  "what varchar(255) not null,"        /* What they did. */
  "modTime timestamp not null)";        /* Modification time. */


void hgSetDb(char *dbName)
/* Set the database name. */
{
hSetDb(dbName);
}

char *hgGetDb()
/* Return the current database name. */
{
return hGetDb();
}

struct sqlConnection *hgAllocConn()
/* Get free connection if possible. If not allocate a new one. */
{
return hAllocConn();
}

void hgFreeConn(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
hFreeConn(pConn);
}

HGID hgIdQuery(struct sqlConnection *conn, char *query)
/* Return first field of first table as HGID. 0 return ok. */
{
struct sqlResult *sr;
char **row;
HGID ret = 0;

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find ID in response to %s\nDatabase needs updating", query);
ret = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return ret;
}

HGID hgRealIdQuery(struct sqlConnection *conn, char *query)
/* Return first field of first table as HGID- abort if 0. */
{
HGID ret = hgIdQuery(conn, query);
if (ret == 0)
    errAbort("Unexpected NULL response to %s", query);
return ret;
}

HGID hgGetMaxId(struct sqlConnection *conn, char *tableName)
/* get the maximum value of the id column in a table or zero if empry  */
{
/* we get a row with NULL if the table is empty */
char query[128];
char **row = NULL;
HGID maxId;
struct sqlResult *sr;

safef(query, sizeof(query), "SELECT MAX(id) from %s", tableName);

sr = sqlGetResult(conn, query);
if (sr != NULL)
    row = sqlNextRow(sr);
if ((row == NULL) || (row[0] == NULL))
    maxId = 0;  /* empty table */
else
    maxId = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return maxId;
}

static HGID startUpdateId;	/* First ID in this update. */
static HGID endUpdateId;	/* One past last ID in this update. */

HGID hgNextId()
/* Get next free global ID. */
{
return endUpdateId++;
}

struct sqlConnection *hgStartUpdate()
/* Open and connection and get next global id from the history table */
{
static boolean initialized = FALSE;
struct sqlConnection *conn = sqlConnect(hgGetDb());

if (!initialized)
    {
    if (sqlMaybeMakeTable(conn, "history", historyCreate))
        sqlUpdate(conn, "INSERT into history VALUES(NULL,0,10,USER(),'New',NOW())");
    initialized = TRUE;
    }

startUpdateId = endUpdateId = hgIdQuery(conn, 
    "SELECT MAX(endId) from history");
return conn;
}

void hgEndUpdate(struct sqlConnection **pConn, char *comment, ...)
/* Finish up connection with a printf format comment. */
{
struct sqlConnection *conn = *pConn;
struct dyString *query = newDyString(256);
va_list args;
va_start(args, comment);

dyStringPrintf(query, "INSERT into history VALUES(NULL,%d,%d,USER(),\"", 
	startUpdateId, endUpdateId);
dyStringVaPrintf(query, comment, args);
dyStringAppend(query, "\",NOW())");
sqlUpdate(conn,query->string);
sqlDisconnect(pConn);
}

static void getTabFile(char *tmpDir, char *tableName, char *path)
/* generate path to tab file */
{
strcpy(path, tmpDir);
strcat(path, "/");
strcat(path, tableName);
strcat(path, ".tab");
}

FILE *hgCreateTabFile(char *tmpDir, char *tableName)
/* Open a tab file with name corresponding to tableName in tmpDir. */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
return mustOpen(path, "w");
}

void hgLoadTabFile(struct sqlConnection *conn, char *tmpDir, char *tableName,
                   FILE **tabFh)
/* Load tab delimited file corresponding to tableName. close fh if not NULL */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
carefulClose(tabFh);
sqlLoadTabFile(conn, path, tableName, SQL_TAB_FILE_WARN_ON_ERROR);
}

void hgRemoveTabFile(char *tmpDir, char *tableName)
/* Remove file. */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
if (remove(path < 0))
    errnoAbort("Couldn't remove %s", path);
}


int hgAddToExtFile(char *path, struct sqlConnection *conn)
/* Add entry to ext file table.  Delete it if it already exists. 
 * Returns extFile id. */
{
static boolean initialized = FALSE;
char root[128], ext[64], name[256];
struct dyString *dy = newDyString(1024);
long long size = fileSize(path);
HGID id = hgNextId();

/* create table if it doesn't exist */
if (!initialized)
    {
    sqlMaybeMakeTable(conn, "extFile", extFileCreate);
    initialized = TRUE;
    }

/* Construct file name without the directory. */
splitPath(path, NULL, root, ext);
safef(name, sizeof(name), "%s%s", root, ext);

/* Delete it from database. */
dyStringPrintf(dy, "delete from extFile where path = '%s'", path);
sqlUpdate(conn, dy->string);

/* Add it to table. */
dyStringClear(dy);
dyStringPrintf(dy, "INSERT into extFile VALUES(%u,'%s','%s',%lld)",
    id, name, path, size);
sqlUpdate(conn, dy->string);

dyStringFree(&dy);
return id;
}

