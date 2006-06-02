/* Delete ignored entries from the database.  Also handled deleting of
 * accessions to explictly reloadd */
#include "common.h"
#include "gbDefs.h"
#include "gbIgnoreDelete.h"
#include "gbRelease.h"
#include "gbIgnore.h"
#include "sqlDeleter.h"
#include "gbDefs.h"
#include "gbStatusTbl.h"
#include "gbVerb.h"
#include "hdb.h"
#include "gbFileOps.h"
#include "gbMetaData.h"
#include "gbAlignData.h"

static void deleteFromTables(struct sqlConnection *conn,
                             struct sqlDeleter* deleter, unsigned srcDb)
/* deleted acc in a sqlDeleter from all tables. */
{
/* order is important here */
gbAlignDataDeleteFromTables(conn, srcDb, GB_MRNA, deleter);
gbAlignDataDeleteFromTables(conn, srcDb, GB_EST, deleter);

gbMetaDataDeleteFromIdTables(conn, deleter);
gbMetaDataDeleteFromTables(conn, srcDb, deleter);
sqlDeleterDel(deleter, conn, "gbStatus", "acc");
}


static boolean inGbStatusTable(struct sqlConnection *conn, char* acc,
                               time_t modDate)
/* check if the specified accession is in the gbStatus table */
{
char query[512];
safef(query, sizeof(query),
      "SELECT count(*) FROM gbStatus WHERE (acc='%s') AND (modDate='%s')",
      acc, gbFormatDate(modDate));
return (sqlQuickNum(conn, query) > 0);
}

static struct sqlDeleter*  buildIgnoredDeleters(struct sqlConnection *conn,
                                                struct gbRelease* release,
                                                boolean force, char* workDir)
/* Construct a deleter object with ignored acc that are in gbStatus.  return
 * NULL if none. */
{
struct sqlDeleter* deleter = NULL;
struct hashCookie cookie;
struct hashEl* hel;
char tmpDir[PATH_LEN];

/* Need to force load of ignore table, as release might not be initialized yet */
gbReleaseLoadIgnore(release);

safef(tmpDir, sizeof(tmpDir), "%s/ignore", workDir);

/* build delete object */
cookie = hashFirst(release->ignore->accHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct gbIgnoreAcc* igAcc;
    for (igAcc = hel->val; igAcc != NULL; igAcc = igAcc->next)
        {
        if (force || inGbStatusTable(conn, igAcc->acc, igAcc->modDate))
            {
            if (deleter == NULL)
                deleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
            sqlDeleterAddAcc(deleter, igAcc->acc);
            gbVerbMsg(4, "%s %s ignored, will delete", igAcc->acc, 
                      gbFormatDate(igAcc->modDate));
            }

        }
    }
return deleter;
}

static void deleteIgnoredRelease(struct gbRelease* release, boolean force, char* workDir)
/* deleted any sequences in the ignore table from the database that are
 * in the gbStatus table for a release. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlDeleter* deleter =  NULL;

if (sqlTableExists(conn, GB_STATUS_TBL))
    deleter = buildIgnoredDeleters(conn, release, force, workDir);

/* drop what we found in the gbStatus table */
if (deleter != NULL)
    {
    deleteFromTables(conn, deleter, release->srcDb);
    sqlDeleterFree(&deleter);
    }
hFreeConn(&conn);
}

void gbIgnoredDelete(struct gbSelect* selectList, boolean force, char* workDir)
/* Deleted any sequences in the ignore table from the database that are in the
 * gbStatus table for all selected releases. force will delete even if not in
 * gbStatus. */
{
struct gbSelect* select = selectList;
struct gbRelease *release = NULL;

/* only call one per release, this loop works because list is grouped by
 * release */
for (; select != NULL; select = select->next)
    {
    if (select->release != release)
        {
        deleteIgnoredRelease(select->release, force, workDir);
        release = select->release;
        }
    }
}

static struct sqlDeleter* buildReloadDeleter(char *reloadList, unsigned srcDb, char *tmpDir)
/* read reload list, building a deleter for the specified source DB */
{
struct sqlDeleter* deleter = NULL;
struct lineFile *lf = gzLineFileOpen(reloadList);
char *row[1];

while (lineFileChopNext(lf, row, ArraySize(row)))
    {
    char *acc = trimSpaces(row[0]);
    if (gbGuessSrcDb(acc) == srcDb)
        {
        if (deleter == NULL)
            deleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
        sqlDeleterAddAcc(deleter, acc);
        gbVerbMsg(3, "%s delete for reloading", acc);
        }
    }
gzLineFileClose(&lf);
return deleter;
}

void gbReloadDelete(char *reloadList, char* workDir)
/* delete sequences that have been explictly requested for reloading */
{
struct sqlConnection *conn = hAllocConn();
char tmpDir[PATH_LEN];
struct sqlDeleter* deleter;
safef(tmpDir, sizeof(tmpDir), "%s/reload", workDir);

/* delete genbanks to reload */
deleter = buildReloadDeleter(reloadList, GB_GENBANK, tmpDir);
if (deleter != NULL)
    {
    deleteFromTables(conn, deleter, GB_GENBANK);
    sqlDeleterFree(&deleter);
    }

/* delete refseqs to reload */
deleter = buildReloadDeleter(reloadList, GB_REFSEQ, tmpDir);
if (deleter != NULL)
    {
    deleteFromTables(conn, deleter, GB_REFSEQ);
    sqlDeleterFree(&deleter);
    }
hFreeConn(&conn);
}


/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
