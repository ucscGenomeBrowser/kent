/* hgData_db - functions to read dbDb and chromInfo. */
#include "common.h"
#include "hgData.h"
#include "hdb.h"
#include "chromInfo.h"
#include "trackDb.h"

static char const rcsid[] = "$Id: hgData_db.c,v 1.1.2.4 2009/02/25 11:22:56 mikep Exp $";

static struct dbDbClade *dbDbCladeLoad(char **row)
/* Load a dbDbClade from row fetched with select * from dbDb
 * from genome database.  Dispose of this with dbDbCladeFree(). */
{
struct dbDbClade *ret;

AllocVar(ret);
ret->clade = cloneString(row[0]);
ret->genome = cloneString(row[1]);
ret->description = cloneString(row[2]);
ret->name = cloneString(row[3]);
ret->organism = cloneString(row[4]);
ret->scientificName = cloneString(row[5]);
ret->taxId = sqlSigned(row[6]);
ret->sourceName = cloneString(row[7]);
ret->defaultPos = cloneString(row[8]);
return ret;
}

static void dbDbCladeFree(struct dbDbClade **pEl)
/* Free a single dynamically allocated dbDbClade such as created
 * with dbDbCladeLoad(). */
{
struct dbDbClade *el;

if ((el = *pEl) == NULL) return;
freeMem(el->clade);
freeMem(el->genome);
freeMem(el->description);
freeMem(el->name);
freeMem(el->organism);
freeMem(el->scientificName);
freeMem(el->sourceName);
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


struct dbDbClade *hGetIndexedDbClade(char *genome)
/* Get list of active genome databases and clade
 * Only get details for one 'genome' unless NULL
 * in which case get all databases.
 * Dispose of this with dbDbCladeFreeList. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct dbDbClade *dbList = NULL, *dbs;

/* Scan through dbDb table, loading into list */
if (genome)
  {
    char query[1024];
    safef(query, sizeof(query), "\
SELECT c.label, g.genome, description, d.name, organism, scientificName, 0 as taxId, sourceName, defaultPos \
FROM dbDb d join genomeClade g using (genome) join clade c on g.clade=c.name \
WHERE d.active = 1 AND d.name=\"%s\" \
ORDER BY c.priority,g.priority,d.orderKey", genome);
    sr = sqlGetResult(conn, query);
  }
else
  {
  sr = sqlGetResult(conn, "\
SELECT c.label, g.genome, description, d.name, organism, scientificName, 0 as taxId, sourceName, defaultPos \
FROM dbDb d join genomeClade g using (genome) join clade c on g.clade=c.name \
WHERE d.active = 1 \
ORDER BY c.priority,g.priority,d.orderKey");
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

time_t hGetLatestUpdateTimeDbClade()
// return the latest time that any of the relevant tables were changed
{
struct sqlConnection *conn = hConnectCentral();
time_t latest = max( max(
    sqlTableUpdateTime(conn, "dbDb"), 
    sqlTableUpdateTime(conn, "genomeClade")), 
    sqlTableUpdateTime(conn, "clade"));
hDisconnectCentral(&conn);
return latest;
}

time_t hGetLatestUpdateTimeChromInfo(char *db)
// return the latest time that the chromInfo table was changed
{
struct sqlConnection *conn = hAllocConn(db);
time_t latest = sqlTableUpdateTime(conn, "chromInfo");
hFreeConn(&conn);
return latest;
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

