/* hgData_db - functions to read dbDb and chromInfo. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_db.c,v 1.1.2.1 2009/01/13 15:48:53 mikep Exp $";

static struct dbDbClade *dbDbCladeLoad(char **row)
/* Load a dbDbClade from row fetched with select * from dbDb
 * from database.  Dispose of this with dbDbCladeFree(). */
{
struct dbDbClade *ret;

AllocVar(ret);
ret->name = cloneString(row[0]);
ret->description = cloneString(row[1]);
ret->organism = cloneString(row[2]);
ret->genome = cloneString(row[3]);
ret->scientificName = cloneString(row[4]);
ret->sourceName = cloneString(row[5]);
ret->clade = cloneString(row[6]);
ret->defaultPos = cloneString(row[7]);
ret->orderKey = sqlSigned(row[8]);
ret->priority = atof(row[9]);
return ret;
}

static void dbDbCladeFree(struct dbDbClade **pEl)
/* Free a single dynamically allocated dbDbClade such as created
 * with dbDbCladeLoad(). */
{
struct dbDbClade *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->description);
freeMem(el->organism);
freeMem(el->genome);
freeMem(el->scientificName);
freeMem(el->sourceName);
freeMem(el->clade);
freeMem(el->defaultPos);
freez(pEl);
}

void dbDbCladeFreeList(struct dbDbClade **pList)
/* Free a list of dynamically allocated dbDbClade's */
{
struct dbDbClade *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dbDbCladeFree(&el);
    }
*pList = NULL;
}

struct dbDbClade *hGetIndexedDbClade(char *db)
/* Get list of active databases and clade
 * Only get details for one 'db' unless NULL
 * in which case get all databases.
 * Dispose of this with dbDbCladeFreeList. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct dbDbClade *dbList = NULL, *dbs;

/* Scan through dbDb table, loading into list */
if (db)
  {
    char query[1024];
    safef(query, sizeof(query), "SELECT name, description, organism, dbDb.genome, scientificName, sourceName, clade, defaultPos, orderKey, priority FROM dbDb,genomeClade WHERE dbDb.active = 1 and dbDb.genome = genomeClade.genome and dbDb.name=\"%s\"", db);
    sr = sqlGetResult(conn, query);
  }
else
  {
  sr = sqlGetResult(conn, "SELECT name, description, organism, dbDb.genome, scientificName, sourceName, clade, defaultPos, orderKey, priority FROM dbDb,genomeClade WHERE dbDb.active = 1 and dbDb.genome = genomeClade.genome ORDER BY clade,priority,orderKey");
  }
while ((row = sqlNextRow(sr)) != NULL)
    {
    dbs = dbDbCladeLoad(row);
    slAddHead(&dbList, dbs);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slReverse(&dbList);
return dbList;
}

struct chromInfo *getAllChromInfo(char *db)
/* Query db.chromInfo for all chrom info. */
{
struct chromInfo *ci = NULL;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr = NULL;
char **row = NULL;
sr = sqlGetResult(conn, "select * from chromInfo order by chrom desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    slSafeAddHead(&ci, chromInfoLoad(row));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ci;
}

