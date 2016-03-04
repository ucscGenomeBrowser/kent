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


static char extFileCreate[] =
/* This keeps track of external files and directories. */
"create table %s ("
  "id int unsigned not null primary key,"  /* Unique ID across all tables. */
  "name varchar(64) not null,"	  /* Symbolic name of file.  */
  "path varchar(255) not null,"   /* Full path. Ends in '/' if a dir. */
  "size bigint unsigned not null,"           /* Size of file (checked) */
                   /* Extra indices. */
  "index (name))";

static char historyCreate[] =	
/* This contains a row for each update made to database. */
NOSQLINJ "create table history ("
  "ix int not null auto_increment primary key,"  /* Update number. */
  "startId int unsigned not null,"              /* Start this session's ids. */
  "endId int unsigned not null,"                /* First id for next session. */
  "who varchar(255) not null,"         /* User who updated. */
  "what varchar(255) not null,"        /* What they did. */
  "modTime timestamp not null,"        /* Modification time. */
  "errata varchar(255) )";            /* Deleted data */

HGID hgGetMaxId(struct sqlConnection *conn, char *tableName)
/* get the maximum value of the id column in a table or zero if empry  */
{
/* we get a row with NULL if the table is empty */
char query[128];
char **row = NULL;
HGID maxId;
struct sqlResult *sr;

sqlSafef(query, sizeof(query), "SELECT MAX(id) from %s", tableName);

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

static void ensureHistoryTableExists(struct sqlConnection *conn)
/* create history table if it doesn't exist */
{
static boolean first = TRUE;
if (first)
    {
    sqlMaybeMakeTable(conn, "history", historyCreate);
    first = FALSE;
    }
}

static void hgHistoryEnterVa(struct sqlConnection *conn, int startId, int endId, char *comment, va_list args)
/* add an entry to the history table */
{
// create SQL encoded string for comment
struct dyString *commentBuf = dyStringNew(256);
dyStringVaPrintf(commentBuf, comment, args);
struct dyString *query = dyStringNew(256);
ensureHistoryTableExists(conn);
sqlDyStringPrintf(query, "INSERT into history (ix, startId, endId, who, what, modTime, errata) VALUES (NULL,%d,%d,'%s','%s',NOW(), NULL)",
               startId, endId, getUser(), commentBuf->string);
sqlUpdate(conn,query->string);
dyStringFree(&query); 
dyStringFree(&commentBuf);
}

void hgHistoryComment(struct sqlConnection *conn, char *comment, ...)
/* Add comment to history table. */
{
va_list args;
va_start(args, comment);
hgHistoryEnterVa(conn, 0, 0, comment, args);
va_end(args);
}

void hgHistoryCommentWithIds(struct sqlConnection *conn, int startId, int endId, char *comment, ...)
/* Add comment to history table, with id range. */
{
va_list args;
va_start(args, comment);
hgHistoryEnterVa(conn, startId, endId, comment, args);
va_end(args);
}

struct sqlConnection *hgStartUpdate(char *db)
/* Open and connection and lock the history table */
{
struct sqlConnection *conn = sqlConnect(db);
ensureHistoryTableExists(conn);
sqlGetLock(conn, "history");
return conn;
}

void hgEndUpdate(struct sqlConnection **pConn, int startId, int endId, char *comment, ...)
/* Finish up connection with a printf format comment and optional id range */
{
struct sqlConnection *conn = *pConn;
va_list args;
va_start(args, comment);
hgHistoryEnterVa(conn, startId, endId, comment, args);
va_end(args);
sqlReleaseLock(conn, "history");
sqlDisconnect(pConn);
}

static void getTabFile(char *tmpDir, char *tableName, char path[PATH_LEN])
/* generate path to tab file.  If tmpDir is NULL, use TMPDIR environment, or
 * "/var/tmp".  tmpDir NULL also include pid in file name */
{
boolean inclPid = (tmpDir == NULL);
if (tmpDir == NULL)
    tmpDir = getenv("TMPDIR");
if (tmpDir == NULL)
    tmpDir = "/var/tmp";
if (inclPid)
    safef(path, PATH_LEN, "%s/%s.%d.tab", tmpDir, tableName, (int) getpid()); /* int cast for Solaris */
else
    safef(path, PATH_LEN, "%s/%s.tab", tmpDir, tableName);
}

FILE *hgCreateTabFile(char *tmpDir, char *tableName)
/* Open a tab file with name corresponding to tableName in tmpDir.  If tmpDir is NULL,
 * use TMPDIR environment, or "/var/tmp" */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
return mustOpen(path, "w");
}

int hgUnlinkTabFile(char *tmpDir, char *tableName)
/* Unlink tab file.   If tmpDir is NULL, use TMPDIR environment, or "/var/tmp" */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
return unlink(path);
}

void hgLoadTabFile(struct sqlConnection *conn, char *tmpDir, char *tableName,
                   FILE **tabFh)
/* Load tab delimited file corresponding to tableName. close fh if not NULL.
 * If tmpDir is NULL, use TMPDIR environment, or "/var/tmp"*/
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
carefulClose(tabFh);
sqlLoadTabFile(conn, path, tableName, SQL_TAB_FILE_WARN_ON_ERROR);
}

void hgLoadNamedTabFile(struct sqlConnection *conn, char *tmpDir, char *tableName,
                        char *fileName, FILE **tabFh)
/* Load named tab delimited file corresponding to tableName. close fh if not
 * NULL If tmpDir is NULL, use TMPDIR environment, or "/var/tmp"*/
{
char path[PATH_LEN];
getTabFile(tmpDir, fileName, path);
carefulClose(tabFh);
sqlLoadTabFile(conn, path, tableName, SQL_TAB_FILE_WARN_ON_ERROR);
}

void hgLoadTabFileOpts(struct sqlConnection *conn, char *tmpDir, char *tableName,
                       unsigned options, FILE **tabFh)
/* Load tab delimited file corresponding to tableName. close tabFh if not NULL
 * If tmpDir is NULL, use TMPDIR environment, or "/var/tmp". Options are those
 * supported by sqlLoadTabFile */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
carefulClose(tabFh);
sqlLoadTabFile(conn, path, tableName, options);
}

void hgRemoveTabFile(char *tmpDir, char *tableName)
/* Remove file.* If tmpDir is NULL, use TMPDIR environment, or "/var/tmp" */
{
char path[PATH_LEN];
getTabFile(tmpDir, tableName, path);
if (remove(path) == -1)
    errnoAbort("Couldn't remove %s", path);
}


int hgAddToExtFileTbl(char *path, struct sqlConnection *conn, char *extFileTbl)
/* Add entry to the specified extFile table.  Delete it if it already exists.
 * Returns extFile id. */
{
char root[128], ext[64], name[256];
struct dyString *dy = newDyString(1024);
long long size = fileSize(path);

/* create table if it doesn't exist */
if (!sqlTableExists(conn, extFileTbl))
    {
    char query[1024];
    sqlSafef(query, sizeof(query), extFileCreate, extFileTbl);
    sqlMaybeMakeTable(conn, extFileTbl, query);
    }

HGID id = hgGetMaxId(conn, extFileTbl) + 1;

/* Construct file name without the directory. */
splitPath(path, NULL, root, ext);
safef(name, sizeof(name), "%s%s", root, ext);

/* Delete it from database. */
sqlDyStringPrintf(dy, "delete from %s where path = '%s'", extFileTbl, path);
sqlUpdate(conn, dy->string);

/* Add it to table. */
dyStringClear(dy);
sqlDyStringPrintf(dy, "INSERT into %s (id, name, path, size) VALUES(%u,'%s','%s',%lld)",
               extFileTbl, id, name, path, size);
sqlUpdate(conn, dy->string);
hgHistoryCommentWithIds(conn, id, id, "extFile table %s: added %s (%lld)", extFileTbl, path, size);
dyStringFree(&dy);
return id;
}

int hgAddToExtFile(char *path, struct sqlConnection *conn)
/* Add entry to ext file table.  Delete it if it already exists. 
 * Returns extFile id. */
{
return hgAddToExtFileTbl(path, conn, "extFile");
}

void hgPurgeExtFileTbl(int id, struct sqlConnection *conn, char *extFileTbl)
/* remove an entry from the extFile table.  Called
 * when there is an error loading the referenced file
 */
{
struct dyString *dy = newDyString(1024);

/* Delete it from database. */
sqlDyStringPrintf(dy, "delete from %s where id = '%d'", extFileTbl, id);
sqlUpdate(conn, dy->string);

dyStringFree(&dy);
}

void hgPurgeExtFile(int id,  struct sqlConnection *conn)
/* remove an entry from the extFile table.  Called
 * when there is an error loading the referenced file
 */
{
hgPurgeExtFileTbl(id, conn, "extFile");
}
