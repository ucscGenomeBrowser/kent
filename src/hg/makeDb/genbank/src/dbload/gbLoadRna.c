/* gbLoadRna - load browser database with mRNA/EST info..
 *
 * This loads all partitions in one call to speed up initial load.  by loading
 * gbSeq, etc in a single load data.
 */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "portable.h"
#include "hgRelate.h"
#include "dbLoadOptions.h"
#include "gbConf.h"
#include "gbIndex.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbEntry.h"
#include "gbVerb.h"
#include "gbAligned.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "gbFileOps.h"
#include "gbStatusTbl.h"
#include "gbBuildState.h"
#include "gbMetaData.h"
#include "gbAlignData.h"
#include "gbLoadedTbl.h"
#include "gbIgnoreDelete.h"
#include "gbSql.h"
#include "sqlUpdater.h"
#include "sqlDeleter.h"
#include "extFileTbl.h"
#include "dbLoadPartitions.h"
#include <signal.h>

static char const rcsid[] = "$Id: gbLoadRna.c,v 1.37 2008/03/29 04:46:51 markd Exp $";

/* FIXME: add optimize subcommand to sort all alignment tables */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"drop", OPTION_BOOLEAN},
    {"move", OPTION_BOOLEAN},
    {"copy", OPTION_BOOLEAN},
    {"release", OPTION_STRING},
    {"srcDb", OPTION_STRING},
    {"type", OPTION_STRING},
    {"orgCat", OPTION_STRING},
    {"accPrefix", OPTION_STRING},
    {"workdir", OPTION_STRING},
    {"goFaster", OPTION_BOOLEAN},
    {"dryRun", OPTION_BOOLEAN},
    {"initialLoad", OPTION_BOOLEAN},
    {"maxShrinkage", OPTION_FLOAT},
    {"allowLargeDeletes", OPTION_BOOLEAN},
    {"gbdbGenBank", OPTION_STRING},
    {"forceIgnoreDelete", OPTION_BOOLEAN},
    {"rebuildDerived", OPTION_BOOLEAN},
    {"reloadList", OPTION_STRING},
    {"reload", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {"conf", OPTION_STRING},
    {NULL, 0}
};


/* command list parameters */
static char *gDatabase = NULL;        /* database to load */
static char* gGbdbGenBank = NULL;     /* root file path to put in database */
static float gMaxShrinkage = 0.1;     /* restriction on number deleted */
static char* gWorkDir = NULL;         /* tmp directory */
static boolean gReload = FALSE;       /* reload the select categories */
static boolean gForceIgnoreDelete = FALSE; /* force entries in ignore.idx to
                                            * be dropped even if not in gbStatus */
static struct dbLoadOptions gOptions; /* options from cmdline and conf */

/* other globals */
static boolean gStopSignaled = FALSE;  /* stop at the end of the current
                                        * partition */
static boolean gMaxShrinkageError = FALSE;  /* exceeded maxShrinkage 
                                             * in some partition */
static unsigned gExtFileChged = 0;      /* total number of extFile changes
                                         * done */

/* in initialLoad mode, pending updates */
static struct sqlUpdater* gPendingStatusUpdates = NULL;

/* loaded updates table */
static struct gbLoadedTbl* gLoadedTbl = NULL;

void checkForStop()
/* called at safe places to check for stop request, either from a signal
 * or var/dbload.stop existing */
{
static char* STOP_FILE = "var/dbload.stop";
if (gStopSignaled)
    {
    fprintf(stderr, "*** Stopped by SIGUSR1 request ***\n");
    exit(1);
    }
if (fileExists(STOP_FILE))
    {
    fprintf(stderr, "*** Stopped by %s request ***\n", STOP_FILE);
    exit(1);
    }
}

void checkInitialLoad(struct sqlConnection *conn)
/* verify that certain table don't exist whne initialLoad is specified */
{
static char *checkTbls[] = {
    "gbSeq", "gbExtFile", "gbStatus", NULL
};
int i;
int existCnt = 0;

for (i = 0; checkTbls[i] != NULL; i++)
    {
    if (sqlTableExists(conn, checkTbls[i]))
        existCnt++;
    }

if (existCnt > 0)
    {
    fprintf(stderr, "Error: table already exist:");
    for (i = 0; checkTbls[i] != NULL; i++)
        {
        if (sqlTableExists(conn, checkTbls[i]))
            fprintf(stderr, " %s", checkTbls[i]);
        }
    fprintf(stderr, "\n");
    errAbort("drop tables with -drop or don't specify -initialLoad");
    }

}

void deleteOutdated(struct sqlConnection *conn, struct gbSelect* select,
                    struct gbStatusTbl* statusTbl, char* tmpDir)
/* delete outdated alignments and metadata from the database. */
{
gbVerbEnter(3, "delete outdated");

/* first the alignments */
gbVerbMsg(4, "delete outdated alignments");
gbAlignDataDeleteOutdated(conn, select, statusTbl, &gOptions, tmpDir);

/* now drop metadata entries */
gbVerbMsg(4, "delete outdated metadata");
gbMetaDataDeleteOutdated(conn, select, statusTbl, &gOptions, tmpDir);

/* Now it's safe to drop deleted entries from the database status table. */
gbVerbMsg(4, "delete outdated gbStatus");
gbStatusTblRemoveDeleted(statusTbl, conn);

/* orphaned now become new */
statusTbl->newList = slCat(statusTbl->newList, statusTbl->orphanList);
statusTbl->orphanList = NULL;
statusTbl->numNew += statusTbl->numOrphan;
statusTbl->numOrphan = 0;

gbVerbLeave(3, "delete outdated");
}

bool updateHasAligned(struct gbSelect* select, struct gbUpdate* update)
/* check if the particular update has any alignments for the release
 * and srcDb.  This checks for indices being readable, allowing
 * them to be hidden. */
{
struct gbUpdate *updateHold = select->update;
unsigned orgCatsHold = select->orgCats;
unsigned hasAligns = FALSE;

if (gbProcessedGetIndex(select, NULL))
    {
    select->orgCats = GB_NATIVE;
    if (gbAlignedGetIndex(select, NULL))
        hasAligns = TRUE;
    select->orgCats = GB_XENO;
    if (gbAlignedGetIndex(select, NULL))
        hasAligns = TRUE;
    }

select->update = updateHold;
select->orgCats = orgCatsHold;
return hasAligns;
}

bool updateNeedsLoaded(struct gbSelect* select, struct gbUpdate* update)
/* check if an update and orgCat is not in the gbLoaded table, but does
 * have index files. */
{
struct gbUpdate *updateHold = select->update;
boolean needsLoaded = FALSE;
select->update = update;

/* If the table indicates that this partition has not been loaded or extFile
 * links need to be updated, we check to see if there are readable alignments
 * index files. */
needsLoaded = (!gbLoadedTblIsLoaded(gLoadedTbl, select))
    && updateHasAligned(select, update);

select->update = updateHold;
return needsLoaded;
}

bool anyUpdatesNeedLoaded(struct gbSelect* select)
/* Determine if any update containing data in selected partition is not in the
 * gbLoaded table.  If it all are in the table, we don't have to bother doing
 * the per-sequence check. */
{
struct gbUpdate* update;

for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (updateNeedsLoaded(select, update))
        return TRUE;
    }
return FALSE;
}

void updateLoadedTblForUpdate(struct gbSelect* select, 
                              struct gbUpdate* update)
/* update the loaded table for a partition and update */
{
struct gbUpdate *updateHold = select->update;
select->update = update;

/* Only add if not already there and there are alignments. */
if (!gbLoadedTblHasEntry(gLoadedTbl, select)
    && updateHasAligned(select, update))
    gbLoadedTblAdd(gLoadedTbl, select);

select->update = updateHold;
}

void updateLoadedTbl(struct gbSelect* select)
/* update the loaded table for this partition, adding all updates that
 * don't already exist. */
{
struct gbUpdate* update;

for (update = select->release->updates; update != NULL; update = update->next)
    updateLoadedTblForUpdate(select, update);
}

void parseUpdateMetaData(struct sqlConnection *conn,
                         struct gbSelect* select, 
                         struct gbStatusTbl* statusTbl)
/* Parse metadata for changed and new entry for an update.  Done one
 * update at a time to allow reading the ra file in sequential order
 * (as there is one per update).  This doesn't load the mrna or seq
 * tables, but might add to the unique string tables. */
{
gbVerbEnter(4, "process metadata for %s", gbSelectDesc(select));
gbMetaDataProcess(conn, statusTbl, select);
gbUpdateClearSelectVer(select->update);
gbVerbLeave(4, "process metadata for %s", gbSelectDesc(select));
}

void loadMetaData(struct sqlConnection *conn)
/* load the metadata into the database */
{
gbVerbEnter(3, "loading metadata");
gbMetaDataDbLoad(conn);
gbVerbLeave(3, "loading metadata");
}

void processMetaData(struct sqlConnection *conn, struct gbSelect* select,
                     struct gbStatusTbl* statusTbl, char* tmpDir)
/* Parse and load the metadata */
{
struct gbUpdate* update;

gbVerbEnter(3, "processing metadata");
gbMetaDataInit(conn, select->release->srcDb, &gOptions, 
               gGbdbGenBank, tmpDir);

for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (update->selectProc)
        {
        select->update = update;
        parseUpdateMetaData(conn, select, statusTbl);
        }
    }
select->update = NULL;
gbVerbLeave(3, "processing metadata");
if (!(gOptions.flags & DBLOAD_INITIAL))
    {
    gbMetaDataUpdateChgGenes(conn, select, statusTbl, tmpDir);
    loadMetaData(conn);
    }
}

void processUpdateAlignsForOrgCat(struct sqlConnection *conn,
                                  struct gbSelect* select, unsigned orgCat,
                                  struct gbStatusTbl* statusTbl)
/* process alignments for an organism category, if it is selected */
{
if ((orgCat & select->orgCats)
    && (gbUpdateGetNumAligns(select->update, select->type, orgCat) > 0))
    {
    unsigned orgCatsHold = select->orgCats;
    select->orgCats = orgCat;
    gbAlignDataProcess(conn, select, statusTbl);
    select->orgCats = orgCatsHold;
    }
}

void processUpdateAligns(struct sqlConnection *conn, struct gbSelect* select,
                         struct gbUpdate* update,
                         struct gbStatusTbl* statusTbl)
/* Get alignements for an update.  */
{
select->update = update;
gbVerbEnter(4, "process alignments: %s", gbSelectDesc(select));

if (select->orgCats & GB_NATIVE)
    processUpdateAlignsForOrgCat(conn, select, GB_NATIVE, statusTbl);
if (select->orgCats & GB_XENO)
    processUpdateAlignsForOrgCat(conn, select, GB_XENO, statusTbl);

gbUpdateClearSelectVer(select->update);

gbVerbLeave(4, "process alignments: %s", gbSelectDesc(select));
select->update = NULL;
}

void loadAligns(struct sqlConnection *conn)
/* load pending alignments */
{
gbVerbEnter(3, "load alignments");
gbAlignDataDbLoad(conn);
gbVerbLeave(3, "load alignments");
}

boolean updateSelected(struct gbUpdate* update, struct gbSelect* select)
/* check if an update is selected and has alignments based on selection
 * criteria */
{
int typeIdx = gbTypeIdx(select->type);
if (!update->selectAlign)
    return FALSE;  /* not selected */

return ((select->orgCats & GB_NATIVE)
        && (update->numNativeAligns[typeIdx] > 0))
    || ((select->orgCats & GB_XENO)
        && (update->numXenoAligns[typeIdx] > 0));

}

void processAligns(struct sqlConnection *conn, struct gbSelect* select,
                   struct gbStatusTbl* statusTbl, char* tmpDir)
/* load the alignments.  select->orgCat determines if native and/or xeno
* are loaded */
{
struct gbUpdate* update;

gbVerbEnter(3, "processing alignments");

gbAlignDataInit(tmpDir, &gOptions, conn);
/* load alignments for updates that are new and actually had sequences align */
for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (updateSelected(update, select))
        processUpdateAligns(conn, select, update, statusTbl);
    }
select->update = NULL;
gbAlignDataSetStatus(statusTbl);
gbVerbLeave(3, "processing alignments");
if ((gOptions.flags & DBLOAD_INITIAL) == 0)
    loadAligns(conn);
}

void databaseUpdate(struct gbSelect* select)
/* update the database from genbank state on disk */
{
struct sqlConnection *conn = hAllocConn();
struct gbStatusTbl* statusTbl;
boolean maxShrinkageExceeded;
char typePrefix[32], tmpDir[PATH_LEN];

gbVerbEnter(3, "update %s", gbSelectDesc(select));

/* Setup tmp dir for load, must be unique for each update due to
 * initialLoad feature */
if (select->accPrefix != NULL)
    safef(typePrefix, sizeof(typePrefix), "%s.%s", gbFmtSelect(select->type),
          select->accPrefix);
else
    safef(typePrefix, sizeof(typePrefix), "%s", gbFmtSelect(select->type));

safef(tmpDir, sizeof(tmpDir), "%s/%s/%s/%s",
      gWorkDir, select->release->name, select->release->genome->database,
      typePrefix);
if (!(gOptions.flags & DBLOAD_DRY_RUN))
    gbMakeDirs(tmpDir);


/* Build list of entries that need processed.  This also flags updates that
 * have the change and new entries so we can limit the per-update processing.
 */
statusTbl = gbBuildState(conn, select, &gOptions, gMaxShrinkage, tmpDir,
                         gbVerbose, FALSE, &maxShrinkageExceeded);
if (maxShrinkageExceeded)
    {
    fprintf(stderr, "Warning: switching to dryRun mode due to maxShrinkage being exceeded\n");
    gMaxShrinkageError = TRUE;
    gOptions.flags |= DBLOAD_DRY_RUN;
    }
if (gOptions.flags & DBLOAD_DRY_RUN)
    {
    gbVerbLeave(3, "dry run, skipping update %s", gbSelectDesc(select));
    gbStatusTblFree(&statusTbl);
    hFreeConn(&conn);
    return;
    }

checkForStop(); /* last safe place */

/* count global number of extFileChgs */
gExtFileChged += statusTbl->numExtChg;

/* first clean out old and changed */
deleteOutdated(conn, select, statusTbl, tmpDir);

/* meta data MUST be done first, it sets some gbStatus data */
processMetaData(conn, select, statusTbl, tmpDir);
processAligns(conn, select, statusTbl, tmpDir);

/* now it's safe to update the status table, delay commit for initialLoad */
if (gOptions.flags & DBLOAD_INITIAL)
    slSafeAddHead(&gPendingStatusUpdates,
                  gbStatusTblUpdate(statusTbl, conn, FALSE));
else
    gbStatusTblUpdate(statusTbl, conn, TRUE);

/* add this and partition to the loaded table, if not already there.
 * set the extFile updated flag updates were done or this is the initial load    */
updateLoadedTbl(select);
if (gOptions.flags & DBLOAD_INITIAL)
    gbLoadedTblSetExtFileUpdated(gLoadedTbl, select);

if ((gOptions.flags & DBLOAD_INITIAL) == 0)
    gbLoadedTblCommit(gLoadedTbl);

/* print before freeing memory */
gbVerbLeave(3, "update %s", gbSelectDesc(select));
gbStatusTblFree(&statusTbl);

hFreeConn(&conn);
}

void removeForReload(struct gbSelect* select, struct sqlConnection* conn,
                     char* tmpDir)
/* delete all in selected categories.  This is faster than deleting
 * individually */
{
struct sqlDeleter * deleter;
if (select->type & GB_EST)
    errAbort("-reload doesn't handle ESTs (it would be very, very slow),"
             " use -drop instead");

deleter = gbBuildStateReloadDeleter(conn, select,  tmpDir);
if (deleter != NULL)
    {
    gbAlignRemove(conn, &gOptions, select, deleter);
    gbMetaDataRemove(conn, &gOptions, select, deleter);
    sqlDeleterDel(deleter, conn, "gbStatus", "acc");
    }
sqlDeleterFree(&deleter);
}

void doLoadPartition(struct gbSelect* select)
/* Do work of syncing the database with the state in the genbank respository for
 * a given partition.  */
{
gbVerbEnter(2, "load for %s", gbSelectDesc(select));

/* load required entry date */
gbReleaseLoadProcessed(select);
gbReleaseLoadAligned(select);

databaseUpdate(select);

gbVerbLeave(2, "load for %s", gbSelectDesc(select));

/* unload entries to free memory */
gbReleaseUnload(select->release);
}

void loadPartition(struct gbSelect* select, struct sqlConnection* conn)
/* Sync the database with the state in the genbank respository for a given
 * partition of the data.  The gbLoadedTbl is loaded as needed for new
 * release. forceLoad overrides checking the gbLoaded table */
{
checkForStop();  /* don't even get started */

/* Need to make sure this release has been loaded into the gbLoaded obj */
gbLoadedTblUseRelease(gLoadedTbl, select->release);

/* if reloading, clean everything out */
if (gReload)
    removeForReload(select, conn, gWorkDir);

/* do a cheap check to see if it possible there is something
 * to load, before much longer examination of per-sequence tables.*/
if ((gOptions.flags & DBLOAD_BYPASS_GBLOADED) || anyUpdatesNeedLoaded(select))
    doLoadPartition(select);

checkForStop();  /* database committed */
}

void loadDelayedTables()
/* If we are delaying table load, now is the time */
{
struct sqlConnection* conn = hAllocConn();
if (gPendingStatusUpdates == NULL)
    errAbort("no data update were found to loaded with -initialLoad");

loadMetaData(conn);
loadAligns(conn);

while (gPendingStatusUpdates != NULL)
    {
    struct sqlUpdater* statUpd = gPendingStatusUpdates;
    gPendingStatusUpdates = gPendingStatusUpdates->next;
    sqlUpdaterCommit(statUpd, conn);
    sqlUpdaterFree(&statUpd);
    }
gbLoadedTblCommit(gLoadedTbl);
hFreeConn(&conn);
}

void cleanExtFileTable()
/* clean up extFile table if we change references for any seq */
{
struct sqlConnection *conn = hAllocConn();
gbVerbEnter(3, "cleaning extFileTbl");
extFileTblClean(conn, (gbVerbose >= 4));
gbVerbLeave(3, "cleaning extFileTbl");
hFreeConn(&conn);
}

void gbLoadRna(char* reloadList)
/* Sync the database with the state in the genbank respository. */
{
struct gbIndex* index = gbIndexNew(gDatabase, NULL);
struct gbSelect* selectList, *select;
struct sqlConnection* conn;

/* must go through all tables if any reload is selected,
 * extFile update is requested, or rebuilding derived */
if ((reloadList != NULL) || gReload)
    gOptions.flags |= DBLOAD_BYPASS_GBLOADED;

if (gReload && (gOptions.flags & DBLOAD_DRY_RUN))
    errAbort("can't specify both -reload and -dryRun");

gbVerbEnter(1, "gbLoadRna");
conn = hAllocConn();
gbLockDb(conn, NULL);

if (gOptions.flags & DBLOAD_INITIAL)
    checkInitialLoad(conn);

/* delete anything on the reload list up front */
if (((gOptions.flags & DBLOAD_DRY_RUN) == 0) && (reloadList != NULL))
    {
    gbAlignDataInit(gWorkDir, &gOptions, conn);
    gbReloadDelete(reloadList, &gOptions, gWorkDir);
    }

selectList = dbLoadPartitionsGet(&gOptions, index);
if ((gOptions.flags & DBLOAD_INITIAL) && (selectList == NULL))
    errAbort("-initialLoad specified and no sequences were found to load");

/* clean up any ignored entries before setting anything up */
gbVerbEnter(3, "delete ignored");
gbIgnoredDelete(selectList, gForceIgnoreDelete, &gOptions, gWorkDir);
gbVerbLeave(3, "delete ignored");

/* loaded table to track updates that have been processed */
gLoadedTbl = gbLoadedTblNew(conn);

/* load each partition */
for (select = selectList; select != NULL; select = select->next)
    loadPartition(select, conn);

/* If we are delaying table load, now is the time */
if ((gOptions.flags & DBLOAD_INITIAL)
    && ((gOptions.flags & DBLOAD_DRY_RUN) == 0))
    loadDelayedTables();

/* clean up extFile table if we change references for any seq */
if ((gOptions.flags & DBLOAD_EXT_FILE_UPDATE) && ((gOptions.flags & DBLOAD_DRY_RUN) == 0))
    cleanExtFileTable();

/* clean up */
slFreeList(&selectList);
gbMetaDataFree();
gbLoadedTblFree(&gLoadedTbl);
gbUnlockDb(conn, NULL);
hFreeConn(&conn);

/* must go to stderr to be logged */
gbVerbLeave(1, "gbLoadRna");
if (gMaxShrinkageError)
    errAbort("Stoping due to maxShrinkage limit being exceeded in one or more\n"
             "partitions. Investigate and rerun with -allowLargeDeletes.");
}

struct slName* getTableList(struct sqlConnection *conn)
/* get the list of existing tables */
{
struct slName *tables = NULL;
tables = slCat(tables, gbMetaDataListTables(conn));
tables = slCat(tables, gbAlignDataListTables(conn));
gbAddTableIfExists(conn, "gbLoaded", &tables);
gbAddTableIfExists(conn, "gbStatus", &tables); /* must be last */
slReverse(&tables);
return tables;
}

void dropAll(char *database)
/* Drop all gbLoadRna tables from database. */
{
struct slName *tables, *tbl;
struct sqlConnection *conn;

gbVerbEnter(1, "dropAll");
hgSetDb(database);
conn = hAllocConn();
gbLockDb(conn, NULL);

tables = getTableList(conn);
for (tbl = tables; tbl != NULL; tbl = tbl->next)
    sqlDropTable(conn, tbl->name);
slFreeList(&tables);
gbUnlockDb(conn, NULL);
hFreeConn(&conn);
gbVerbLeave(1, "dropAll");
}

void copyTable(struct sqlConnection *conn, char* destDb, char* srcTable,
               char* destTable)
/* copy a table from one database to another */
{
char destDbTable[512], sqlCmd[512];
safef(destDbTable, sizeof(destDbTable), "%s.%s", destDb, destTable);

gbSqlDupTableDef(conn, srcTable, destDbTable);

safef(sqlCmd, sizeof(sqlCmd), "insert into %s select * from %s",
      destDbTable, srcTable);
sqlUpdate(conn, sqlCmd);
}

void copyChromInfo(struct sqlConnection *conn, char* destDb)
/* copy the chromInfo into the dest table if it doesn't already exist.
 * Which not a genbank managed table, this is needed to find the alignments
 * for moving/copying tables. */
{
char destDbTable[512];
safef(destDbTable, sizeof(destDbTable), "%s.%s", destDb, "chromInfo");
if (!sqlTableExists(conn, destDbTable))
    copyTable(conn, destDb, "chromInfo", "chromInfo");
}

void copyAll(char *srcDb, char* destDb)
/* Copy all gbLoadRna tables from database to another. */
{
struct slName *tables, *tbl;
struct sqlConnection *conn;

gbVerbEnter(1, "copyAll");
hgSetDb(srcDb);
conn = hAllocConn();
gbLockDb(conn, srcDb);
gbLockDb(conn, destDb);

copyChromInfo(conn, destDb);

/* copy each table */
tables = getTableList(conn);
for (tbl = tables; tbl != NULL; tbl = tbl->next)
    {
    copyTable(conn, destDb, tbl->name, tbl->name);
    }
slFreeList(&tables);
gbUnlockDb(conn, destDb);
gbUnlockDb(conn, srcDb);
hFreeConn(&conn);
gbVerbLeave(1, "copyAll");
}

void moveAll(char *srcDb, char* destDb)
/* Rename all gbLoadRna tables from database to another. */
{
struct slName *tables, *tbl;
struct sqlConnection *conn;
struct dyString* sqlCmd = dyStringNew(256);
char *sep;

gbVerbEnter(1, "moveAll");
hgSetDb(srcDb);
conn = hAllocConn();
gbLockDb(conn, srcDb);
gbLockDb(conn, destDb);

copyChromInfo(conn, destDb);

/* using one does rename atomically */
tables = getTableList(conn);
dyStringAppend(sqlCmd, "rename table");
sep = " "; /* before first table arg */
for (tbl = tables; tbl != NULL; tbl = tbl->next)
    {
    dyStringPrintf(sqlCmd, "%s%s to %s.%s",
                   sep,tbl->name, destDb, tbl->name);
    sep = ", "; /* before other table arg */
    }
sqlUpdate(conn, sqlCmd->string);
dyStringFree(&sqlCmd);
slFreeList(&tables);

gbUnlockDb(conn, destDb);
gbUnlockDb(conn, srcDb);
hFreeConn(&conn);
gbVerbLeave(1, "moveAll");
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gbLoadRna - load browser database with mRNA/EST info.\n"
  "usage:\n"
  "   gbLoadRna [options] db\n"
  "     db - database for the alignment\n"
  "   gbLoadRna -drop db\n"
  "   gbLoadRna -move db db2\n"
  "   gbLoadRna -copy db db2\n"
  "Options:\n"
  "     -drop - Drop the database tables instead of loading.\n"
  "\n"
  "     -move - move the genbank related tables to another database.\n"
  "      They should not already exist in that database.  This is used\n"
  "      to backup tables when a reload is being done.\n"
  "\n"
  "     -copy - copy the genbank related tables to another database.\n"
  "      They should not already exist in that database.  This is used\n"
  "      to backup tables.\n"
  "\n"
  "     -release=relname - Just load release name (e.g. genbank.131.0),\n"
  "      If not specified, the newest aligned genbank and refseq is loaded.\n"
  "\n"
  "     -srcDb=srcDb - Just load srcDb (genbank or refseq).\n"
  "\n"
  "     -type=type - Just load type (mrna or est).\n"
  "\n"
  "     -orgCat=orgCat - Just load organism restriction (native or xeno).\n"
  "\n"
  "     -accPrefix=ac - Only process ESTs with this 2 char accession prefix.\n"
  "      Must specify -type=est, mostly useful for debugging.\n"
  "\n"
  "     -gbdbGenBank=dir - set gbdb path to dir, default /gbdb/genbank\n"
  "      Can also be set with gbdb.genbank in conf file\n"
  "\n"
  "     -maxShrinkage=frac - specify the maximum shrinkage in the size of the\n"
  "      genbank partition being updated.  If 1.0-(numNew/numOld) > frac,\n"
  "      an error will be generated.  This is to prevent some problem case\n"
  "      from deleting a large number of sequences. Default is 0.1\n"
  "      For very small partitions, used in testing, deleting up to 5 is\n"
  "      allowed if maxShrinkage is not zero. If this limit is exceeded, the\n"
  "      list of accessions to delete is printed and then the loading of that\n"
  "      partition terminated.  Other partitions continue in -dryRun mode,\n"
  "      This allows for examination of the cause of a large deletions without\n"
  "      risking large deletions in other partitions.  The option -allowLargeDeletes\n"
  "      can then be use to allow the deletes to go forwards.\n"
  "\n"
  "     -allowLargeDeletes - disable -maxShrinkage checks.\n"
  "\n"
  "     -goFaster - optimize for speed at the expense of memory\n"
  "\n"
  "     -initialLoad - optimize for initial load of a database.  Incremental\n"
  "      load of tables in batches was much slower (~26 hrs vs ~3 hrs) than\n"
  "      loading an empty table.  This option saves load until all partitions\n"
  "      are processed.  While the would work with incremental load, it\n"
  "      would require more memory.\n"
  "\n"
  "     -workdir=work/load - Temporary directory for load files.\n"
  "\n"
  "     -verbose=n - enable verbose output, values greater than 1 increase \n"
  "                  verbosity:\n"
  "              n >= 1 - basic information about load\n"
  "              n >= 2 - more details\n"
  "              n >= 3 - time spent in various steps\n"
  "              n >= 4 - step details\n"
  "              n >= 5 - information about each selected sequence\n"
  "              n >= 6 - SQL queries\n"
  "\n"
  "     -dryRun - go throught the selection process,  but don't update.\n"
  "      This will still remove ignored accessions.\n"
  "\n"
  "     -forceIgnoreDelete - force entries in ignore.idx to be dropped even\n"
  "      not in gbStatus.  Slow, but a nice way to clean up confusion.\n"
  "\n"
  "     -reloadList=file - File containing sequence ids, one per line, to\n"
  "      remove from the databases before updating.  This causes these\n" 
  "      sequences to be reloaded.  This optional also causes all partitions\n"
  "      to be examined, which slows things down.\n"
  "\n"
  "     -rebuildDerived - rebuild genePred, and gbMiscDiff tables\n"
  "\n"
  "SIGUSR1 will cause process to stop after the current partition.  This\n"
  "will leave the database in a clean state, but the update incomplete.\n"
  );
}

void sigStopSignaled(int sig)
/* signal handler that sets the stopWhenSafe flag */
{
gStopSignaled = TRUE;
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean drop, move, copy;
struct sigaction sigSpec;
setlinebuf(stdout);
setlinebuf(stderr);

ZeroVar(&sigSpec);
sigSpec.sa_handler = sigStopSignaled;
sigSpec.sa_flags = SA_RESTART;
if (sigaction(SIGUSR1, &sigSpec, NULL) < 0)
    errnoAbort("can't set SIGUSR1 handler");

optionInit(&argc, argv, optionSpecs);
drop = optionExists("drop");
move = optionExists("move");
copy = optionExists("copy");
gReload = optionExists("reload");
if (move || copy) 
    {
    if (argc != 3)
        usage();
    }
else if (argc != 2)
    usage();
if ((drop+move+copy) > 1)
    errAbort("can only specify one of -drop, -move, or -copy");

gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 6)
    sqlMonitorEnable(JKSQL_TRACE);
if (drop)
    dropAll(argv[1]);
else if (move)
    moveAll(argv[1], argv[2]);
else if (copy)
    copyAll(argv[1], argv[2]);
else
    {
    char *reloadList = optionVal("reloadList", NULL);
    gDatabase = argv[1];
    gOptions = dbLoadOptionsParse(gDatabase);
    gForceIgnoreDelete = optionExists("forceIgnoreDelete");
    if (optionExists("rebuildDerived"))
        gOptions.flags |= DBLOAD_BYPASS_GBLOADED|DBLOAD_REBUILD_DERIVED;

    gMaxShrinkage = optionFloat("maxShrinkage", 0.1);

    
    gGbdbGenBank = optionVal("gbdbGenBank", NULL);
    if (gGbdbGenBank == NULL)
        gGbdbGenBank = gbConfGet(gOptions.conf, "gbdb.genbank");
    if (gGbdbGenBank == NULL)
        gGbdbGenBank = "/gbdb/genbank";
    gWorkDir = optionVal("workdir", "work/load");

    hgSetDb(gDatabase);
    hSetDb(gDatabase);
    if (gOptions.flags & DBLOAD_DRY_RUN)
        printf("*** using dry run mode ***\n");
    gbLoadRna(reloadList);
    }

return 0;
}
