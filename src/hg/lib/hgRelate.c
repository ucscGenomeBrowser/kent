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

struct sqlConnection *hgFreeConn(struct sqlConnection **pConn)
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


static HGID startUpdateId;	/* First ID in this update. */
static HGID endUpdateId;	/* One past last ID in this update. */

HGID hgNextId()
/* Get next free global ID. */
{
return endUpdateId++;
}

struct sqlConnection *hgStartUpdate()
/* This locks the update table for an update - which prevents anyone else
 * from updating and returns the first usable ID. */
{
struct sqlConnection *conn = sqlConnect(hgGetDb());
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

FILE *hgCreateTabFile(char *tableName)
/* Open a tab file with name corresponding to tableName.  This
 * may just be fclosed when done. (Currently just makes
 * tableName.tab in the current directory.) */
{
char fileName[256];
sprintf(fileName, "%s.tab", tableName);
return mustOpen(fileName, "w");
}

void hgLoadTabFile(struct sqlConnection *conn, char *tableName)
/* Load tab delimited file corresponding to tableName. */
{
char query[512];
char fileName[256];
sprintf(fileName, "%s.tab", tableName);
sprintf(query,
   "LOAD data local infile '%s' into table %s", fileName, tableName);
sqlUpdate(conn, query);
}

