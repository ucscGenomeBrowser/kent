/* gbLoadRna - load browser database with mRNA/EST info.. */

/* This is derived from a system that used to apply to the whole
 * database, but currently only applies to the mRNA/EST bits -
 * where each record has a globally uniq ID across the entire
 * database.   */

#include "common.h"
#include "options.h"
#include "hdb.h"
#include "portable.h"
#include "hgRelate.h"
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
#include "gbIgnore.h"
#include "sqlUpdater.h"
#include "sqlDeleter.h"
#include "seqTbl.h"
#include "extFileTbl.h"
#include <signal.h>

static char const rcsid[] = "$Id: gbLoadRna.c,v 1.1 2003/06/03 01:27:43 markd Exp $";

/* FIXME: add optimize subcommand to sort all alignment tables */
/* FIXME: ignored deletion could be in it's own module */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"drop", OPTION_BOOLEAN},
    {"release", OPTION_STRING},
    {"type", OPTION_STRING},
    {"accPrefix", OPTION_STRING},
    {"workdir", OPTION_STRING},
    {"goFaster", OPTION_BOOLEAN},
    {"dryRun", OPTION_BOOLEAN},
    {"initialLoad", OPTION_BOOLEAN},
    {"maxShrinkage", OPTION_FLOAT},
    {"allowLargeDeletes", OPTION_BOOLEAN},
    {"gbdbGenBank", OPTION_STRING},
    {"noPerChrom", OPTION_BOOLEAN},
    {"reloadList", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};


/* command list parameters */
static char *gDatabase = NULL;        /* database to load */
static boolean gGoFaster = FALSE;     /* optmized for speed */
static boolean gInitialLoad = FALSE;  /* initial load of database */
static boolean gDryRun = FALSE;       /* don't modify the database */
static char* gGbdbGenBank = NULL;     /* root file path to put in database */
static float gMaxShrinkage = 0.1;     /* restriction on number deleted */
static boolean gAllowLargeDeletes = FALSE; /* override maxShrinkage */
static boolean gNoPerChrom = FALSE;   /* don't build per-chrom tables */
static char* gWorkDir = NULL;         /* tmp directory */

/* other globals */
static int gTotalExtChgCnt = 0;  /* total number of extChg seqs processed */
static boolean gStopWhenSafe = FALSE;  /* stop at the end of the current
                                       * partition */
static boolean gMaxShrinkageError = FALSE;  /* exceeded maxShrinkage 
                                             * in some partation */

/* in initialLoad mode, pending updates */
static struct sqlUpdater* gPendingStatusUpdates = NULL;

/* loaded updates table */
static struct gbLoadedTbl* gLoadedTbl = NULL;

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

void deleteFromTables(struct sqlConnection *conn, struct sqlDeleter* deleter,
                      unsigned srcDb)
/* deleted acc in a sqlDeleter from all tables. */
{
/* order is important here */
gbAlignDataDeleteFromTables(conn, srcDb, GB_MRNA, deleter);
gbAlignDataDeleteFromTables(conn, srcDb, GB_EST, deleter);

gbMetaDataDeleteFromIdTables(conn, deleter);
gbMetaDataDeleteFromTables(conn, srcDb, deleter);
sqlDeleterDel(deleter, conn, "gbStatus", "acc");
}

struct sqlDeleter* buildReloadDeleter(char *reloadList, unsigned srcDb, char *tmpDir)
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
            deleter = sqlDeleterNew(tmpDir, (verbose >= 2));
        sqlDeleterAddAcc(deleter, acc);
        gbVerbMsg(1, "%s delete for reloading", acc);
        }
    }
gzLineFileClose(&lf);
return deleter;
}

void deleteReload(char *reloadList)
/* delete sequences that have been explictly requested for reloading */
{
struct sqlConnection *conn = hAllocConn();
char tmpDir[PATH_LEN];
struct sqlDeleter* deleter;
safef(tmpDir, sizeof(tmpDir), "%s/reload", gWorkDir);

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

struct sqlDeleter*  buildIgnoredDeleters(struct sqlConnection *conn,
                                         struct gbRelease* release)
/* Construct a deleter object with ignored acc that are in gbStatus.
 * return NULL if none. */
{
struct sqlDeleter* deleter = NULL;
struct hashCookie cookie;
struct hashEl* hel;
char tmpDir[PATH_LEN];

/* Need to force load of ignore table, as release might not be initialized yet */
gbReleaseLoadIgnore(release);

if (sqlTableExists(conn, GB_STATUS_TBL))
    {
    safef(tmpDir, sizeof(tmpDir), "%s/ignore", gWorkDir);

    /* build delete object */
    cookie = hashFirst(release->ignore->accHash);
    while ((hel = hashNext(&cookie)) != NULL)
        {
        struct gbIgnoreAcc* igAcc = hel->val;
        if (inGbStatusTable(conn, igAcc->acc, igAcc->modDate))
            {
            if (deleter == NULL)
                deleter = sqlDeleterNew(tmpDir, (verbose >= 2));
            sqlDeleterAddAcc(deleter, igAcc->acc);
            gbVerbMsg(1, "%s %s ignored, will delete", igAcc->acc, 
                      gbFormatDate(igAcc->modDate));
            }
        }
    }
return deleter;
}

void deleteIgnoredRelease(struct gbRelease* release)
/* deleted any sequences in the ignore table from the database that are
 * in the gbStatus table for a release. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlDeleter* deleter = buildIgnoredDeleters(conn, release);

/* drop what we found in the gbStatus table */
if (deleter != NULL)
    {
    deleteFromTables(conn, deleter, release->srcDb);
    sqlDeleterFree(&deleter);
    }
hFreeConn(&conn);
}

void deleteIgnored(struct gbSelect* selectList)
/* Deleted any sequences in the ignore table from the database that are
 * in the gbStatus table for all selected releases. */
{
struct gbSelect* select = selectList;
struct gbRelease *release = NULL;

/* this works because list is grouped by release */
for (; select != NULL; select = select->next)
    {
    if (select->release != release)
        {
        deleteIgnoredRelease(select->release);
        release = select->release;
        }
    }
}

boolean haveReleaseType(struct gbSelect* ignoreSelectList,
                        struct gbSelect* select)
/* see if the release and type have been added to the select list */
{
struct gbSelect* ignoreSelect = ignoreSelectList;
for (; ignoreSelect != NULL; ignoreSelect = ignoreSelect->next)
    {
    if ((ignoreSelect->release == select->release)
        && (ignoreSelect->type == select->type))
        return TRUE;
    }
return FALSE;
}

struct gbSelect* getIgnoredSelect(struct gbSelect* selectList)
/* get a list of select objects for all release and type combinations */
{
struct gbSelect* ignoreSelectList = NULL;
struct gbSelect* select = selectList;

for (; select != NULL; select = select->next)
    if (!haveReleaseType(ignoreSelectList, select))
        {
        struct gbSelect* ignoreSelect;
        AllocVar(ignoreSelect);
        ignoreSelect->release = select->release;
        ignoreSelect->type = select->type;
        slAddHead(&ignoreSelectList, ignoreSelect);
        }
return ignoreSelectList;
}

void deleteOutdated(struct sqlConnection *conn, struct gbSelect* select,
                    struct gbStatusTbl* statusTbl, char* tmpDir)
/* delete outdated alignments and metadata from the database. */
{
gbVerbEnter(1, "delete outdated");

/* first the alignments */
gbVerbMsg(1, "delete outdated alignments");
gbAlignDataDeleteOutdated(conn, select, statusTbl, tmpDir);

/* now drop metadata entries */
gbVerbMsg(1, "delete outdated metadata");
gbMetaDataDeleteOutdated(conn, select, statusTbl, tmpDir);

/* Now it's safe to drop deleted entries from the database status table. */
gbVerbMsg(1, "delete outdated gbStatus");
gbStatusTblRemoveDeleted(statusTbl, conn);

/* orphaned now become new */
statusTbl->newList = slCat(statusTbl->newList, statusTbl->orphanList);
statusTbl->orphanList = NULL;
statusTbl->numNew += statusTbl->numOrphan;
statusTbl->numOrphan = 0;

gbVerbLeave(1, "delete outdated");
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

/* If the table indicates that this partition has not been loaded, we check to
 * see if there are readable index files */
needsLoaded = !gbLoadedTblIsLoaded(gLoadedTbl, select)
    && updateHasAligned(select, update);

select->update = updateHold;
return needsLoaded;
}

bool anyUpdatesNeedLoaded(struct gbSelect* select)
/* Determine if any update containing data in selected partation is not in the
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
/* update the loaded table for a partation and update */
{
struct gbUpdate *updateHold = select->update;
select->update = update;

/* only add if update actually has alignments.  Prevents empty or
 * unreadable update directories from being included */
if (!gbLoadedTblIsLoaded(gLoadedTbl, select)
    && updateHasAligned(select, update))
    gbLoadedTblAdd(gLoadedTbl, select);

select->update = updateHold;
}

void updateLoadedTbl(struct gbSelect* select)
/* update the loaded table for this partation, adding all updates that
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
gbVerbEnter(1, "process metadata for %s", gbSelectDesc(select));
gbMetaDataProcess(conn, statusTbl, select);
gbUpdateClearSelectVer(select->update);
gbVerbLeave(1, "process metadata for %s", gbSelectDesc(select));
}

void loadMetaData(struct sqlConnection *conn)
/* load the metadata into the database */
{
gbVerbEnter(1, "loading metadata");
gbMetaDataDbLoad(conn);
gbVerbLeave(1, "loading metadata");
}

void processMetaData(struct sqlConnection *conn, struct gbSelect* select,
                     struct gbStatusTbl* statusTbl, char* tmpDir)
/* Parse and load the metadata */
{
struct gbUpdate* update;

gbVerbEnter(1, "processing metadata");
gbMetaDataInit(conn, select->release->srcDb, gGbdbGenBank, gGoFaster, tmpDir);

for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (update->selectProc)
        {
        select->update = update;
        parseUpdateMetaData(conn, select, statusTbl);
        }
    }
select->update = NULL;
gbVerbLeave(1, "processing metadata");
if (!gInitialLoad)
    loadMetaData(conn);
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
gbVerbEnter(1, "process alignments: %s", gbSelectDesc(select));

processUpdateAlignsForOrgCat(conn, select, GB_NATIVE, statusTbl);
processUpdateAlignsForOrgCat(conn, select, GB_XENO, statusTbl);

gbUpdateClearSelectVer(select->update);

gbVerbLeave(1, "process alignments: %s", gbSelectDesc(select));
select->update = NULL;
}

void loadAligns(struct sqlConnection *conn)
/* load pending alignments */
{
gbVerbEnter(1, "load alignments");
gbAlignDataDbLoad(conn);
gbVerbLeave(1, "load alignments");
}

void processAligns(struct sqlConnection *conn, struct gbSelect* select,
                   struct gbStatusTbl* statusTbl, char* tmpDir)
/* load the alignments */
{
int typeIdx = gbTypeIdx(select->type);
struct gbUpdate* update;

gbVerbEnter(1, "processing alignments");

gbAlignDataInit(tmpDir, gNoPerChrom);
/* load alignments for updates that are new and actually had sequences align */
for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (update->selectAlign &&
        ((update->numNativeAligns[typeIdx] > 0)
         || (update->numXenoAligns[typeIdx] > 0)))
        processUpdateAligns(conn, select, update, statusTbl);
    }
select->update = NULL;
gbAlignDataSetStatus(statusTbl);
gbVerbLeave(1, "processing alignments");
if (!gInitialLoad)
    loadAligns(conn);
}

void databaseUpdate(struct gbSelect* select)
/* update the database from genbank state on disk */
{
struct sqlConnection *conn = hAllocConn();
struct gbStatusTbl* statusTbl;
boolean maxShrinkageExceeded;
char typePrefix[32], tmpDir[PATH_LEN];

gbVerbEnter(1, "update %s", gbSelectDesc(select));

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
if (!gDryRun)
    gbMakeDirs(tmpDir);

/* Build list of entries that need processed.  This also flags updates that
 * have the change and new entries so we can limit the per-update processing.
 */
statusTbl = gbBuildState(conn, select, gInitialLoad, gMaxShrinkage,
                         gAllowLargeDeletes, tmpDir, verbose,
                         &maxShrinkageExceeded);
if (maxShrinkageExceeded)
    {
    fprintf(stderr, "Warning: switching to dryRun mode due to maxShrinkage being exceeded\n");
    gMaxShrinkageError = TRUE;
    gDryRun = TRUE;
    }
if (gDryRun)
    {
    gbVerbLeave(1, "dry run, skipping update %s", gbSelectDesc(select));
    gbStatusTblFree(&statusTbl);
    hFreeConn(&conn);
    return;
    }

/* first clean out old and changed */
deleteOutdated(conn, select, statusTbl, tmpDir);

processMetaData(conn, select, statusTbl, tmpDir);
processAligns(conn, select, statusTbl, tmpDir);

/* now it's safe to update the status table, delay commit for initialLoad */
if (gInitialLoad)
    slSafeAddHead(&gPendingStatusUpdates,
                  gbStatusTblUpdate(statusTbl, conn, FALSE));
else
    gbStatusTblUpdate(statusTbl, conn, TRUE);

/* add this and partation to the loaded table, if not already there */
updateLoadedTbl(select);
if (!gInitialLoad)
    gbLoadedTblCommit(gLoadedTbl, conn);

/* print before freeing memory */
gbVerbLeave(1, "update %s", gbSelectDesc(select));
gTotalExtChgCnt += statusTbl->numExtChg;
gbStatusTblFree(&statusTbl);

hFreeConn(&conn);
}

void doLoadPartition(struct gbSelect* select)
/* Do work of syncing the database with the state in the genbank respository for
 * a given partition.  */
{
gbVerbEnter(1, "load for %s", gbSelectDesc(select));

/* load required entry date */
gbReleaseLoadProcessed(select);
gbReleaseLoadAligned(select);

databaseUpdate(select);

gbVerbLeave(1, "load for %s", gbSelectDesc(select));

/* unload entries to free memory */
gbReleaseUnload(select->release);
}

void loadPartition(struct gbSelect* select, struct sqlConnection* conn,
                   boolean forceLoad)
/* Sync the database with the state in the genbank respository for a given
 * partition of the data.  The gbLoadedTbl is loaded as needed for new
 * release. forceLoad overrides checking the gbLoaded table */
{

/* Need to make sure this release has been loaded into the gbLoaded obj */
gbLoadedTblUseRelease(gLoadedTbl, conn, select->release);

/* do a cheap check to see if it possible there is something
 * to load, before much longer examination of per-sequence tables.*/
if (forceLoad || anyUpdatesNeedLoaded(select))
    doLoadPartition(select);

if (gStopWhenSafe)
    {
    fprintf(stderr, "*** Stopped at user request ***\n");
    exit(1);
    }
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
gbLoadedTblCommit(gLoadedTbl, conn);
hFreeConn(&conn);
}

void cleanExtFileTable()
/* clean up extFile table if we change references for any seq */
{
struct sqlConnection *conn = hAllocConn();
gbVerbEnter(1, "cleaning extFileTbl");
extFileTblClean(conn);
gbVerbLeave(1, "cleaning extFileTbl");
hFreeConn(&conn);
}

void gbLoadRna(char *database, char* relName, char* typesStr, char* limitAccPrefix,
               char* reloadList)
/* Sync the database with the state in the genbank respository.  relName,
 * typesStr and limitAccPrefix may all be null */
{
struct gbIndex* index = gbIndexNew(database, NULL);
unsigned types = (typesStr != NULL) ? gbParseType(typesStr) : GB_MRNA|GB_EST;
struct gbSelect* selectList, *select;
struct sqlConnection* conn;
boolean forceLoad = (reloadList != NULL);
hgSetDb(database);
conn = hAllocConn();

if (gInitialLoad)
    checkInitialLoad(conn);

/* delete anything on the reload list up front */
if (!gDryRun && (reloadList != NULL))
    deleteReload(reloadList);

/* get list of partitions to process */
selectList = gbIndexGetPartitions(index, GB_ALIGNED, (GB_GENBANK|GB_REFSEQ),
                                  relName, types, limitAccPrefix);
if (gInitialLoad && (selectList == NULL))
    errAbort("-initialLoad specified and no sequences were found to load");

/* clean up any ignored entries before setting anything up */
deleteIgnored(selectList);

/* loaded table to track updates that have been processed */
gLoadedTbl = gbLoadedTblNew(conn);

/* load each partation */
for (select = selectList; select != NULL; select = select->next)
    loadPartition(select, conn, forceLoad);

/* If we are delaying table load, now is the time */
if (gInitialLoad && !gDryRun)
    loadDelayedTables();

/* clean up extFile table if we change references for any seq */
if ((gTotalExtChgCnt > 0) && !gDryRun)
    cleanExtFileTable();

/* clean up */
slFreeList(&selectList);
gbMetaDataFree();
gbLoadedTblFree(&gLoadedTbl);
hFreeConn(&conn);

/* must go to stderr to be logged */
gbVerbMsg(0, "gbLoadRna");
if (gMaxShrinkageError)
    errAbort("Stoping due to maxShrinkage limit being exceeded in one or more\n"
             "partitions. Investigate and rerun with -allowLargeDeletes.");
}

void dropAll(char *database)
/* Drop all gbLoadRna tables from database. */
{
struct sqlConnection *conn;
hgSetDb(database);
conn = hAllocConn();

gbMetaDataDrop(conn);
gbAlignDataDrop(conn);
sqlDropTable(conn, "gbLoaded");
sqlDropTable(conn, "gbStatus"); /* must be last */
hFreeConn(&conn);
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
  "Options:\n"
  "     -drop - Drop the database tables instead of loading.\n"
  "\n"
  "     -release=relname - Just load release name (e.g. genbank.131.0),\n"
  "      If not specified, the newest aligned release is loaded.\n"
  "\n"
  "     -type=type - Just load type.\n"
  "\n"
  "     -accPrefix=ac - Only process ESTs with this 2 char accession prefix.\n"
  "      Must specify -type=est, mostly useful for debugging.\n"
  "\n"
  "     -gbdbGenBank=dir - set gbdb path to dir, default /gbdb/genbank\n"
  "\n"
  "     -maxShrinkage=frac - specify the maximum shrinkage in the size of the\n"
  "      genbank partition being updated.  If 1.0-(numNew/numOld) > frac,\n"
  "      an error will be generated.  This is to prevent some problem case\n"
  "      from deleting a large number of sequences. Default is 0.1\n"
  "      For very small partations, used in testing, deleting up to 5 is\n"
  "      allowed if maxShrinkage is not zero. If this limit is exceeded, the\n"
  "      list of accessions to delete is printed and then the loading of that\n"
  "      partation terminated.  Other partations continue in -dryRun mode,\n"
  "      This allows for examination of the cause of a large deletions without\n"
  "      risking large deletions in other partations.  The option -allowLargeDeletes\n"
  "      can then be use to allow the deletes to go forwards.\n"
  "\n"
  "     -allowLargeDeletes - disable -maxShrinkage checks.\n"
  "\n"
  "     -goFaster - optimize for speed at the expense of memory\n"
  "\n"
  "     -initialLoad - optimize for initial load of a database.  Incremental\n"
  "      load of tables in batches was much slower (~26 hrs vs ~3 hrs) than\n"
  "      loading an empty table.  This option saves load until all partations\n"
  "      are processed.  While the would work with incremental load, it\n"
  "      would require more memory.\n"
  "\n"
  "     -workdir=work/load - Temporary directory for load files.\n"
  "      would require more memory.\n"
  "     -noPerChrom - don't build the per-chromosome tables.\n"
  "     -verbose=n - enable verbose output, values greater than 1 increase \n"
  "                  verbosity:\n"
  "              n >= 1 - basic information abourt each step\n"
  "              n >= 2 - more details\n"
  "              n >= 4 - information about each selected sequence\n"
  "              n >= 5 - SQL queries\n"
  "      would require more memory.\n"
  "     -dryRun - go throught the selection process,  but don't update.\n"
  "      This will still remove ignored accessions.\n"
  "     -reloadList=file - File containing sequence ids, one per line, to\n"
  "      remove from the databases before updating.  This causes these\n" 
  "      sequences to be reloaded.  This optional also causes all partations\n"
  "      to be examined, which slows things down.\n"
  "\n"
  "SIGUSR1 will cause process to stop after the current partition.  This\n"
  "will leave the database in a clean state, but the update incomplete.\n"
  );
}

void sigStopWhenSafe(int sig)
/* signal handler that sets the stopWhenSafe flag */
{
gStopWhenSafe = TRUE;
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean drop;
struct sigaction sigSpec;
setlinebuf(stdout);
setlinebuf(stderr);

ZeroVar(&sigSpec);
sigSpec.sa_handler = sigStopWhenSafe;
sigSpec.sa_flags = SA_RESTART;
if (sigaction(SIGUSR1, &sigSpec, NULL) < 0)
    errnoAbort("can't set SIGUSR1 handler");

optionInit(&argc, argv, optionSpecs);
drop = optionExists("drop");
if (argc != 2)
    usage();
gbVerbInit(optionInt("verbose", 0));
if (verbose >= 5)
    sqlTrace = TRUE;  /* global flag in jksql */
if (drop)
    {
    dropAll(argv[1]);
    }
else
    {
    char *relName = optionVal("release", NULL);
    char *typeStr = optionVal("type", NULL);
    char *limitAccPrefix = optionVal("accPrefix", NULL);
    char *reloadList = optionVal("reloadList", NULL);
    gDatabase = argv[1];
    gWorkDir = optionVal("workdir", "work/load");
    if (limitAccPrefix != NULL)
        tolowers(limitAccPrefix);
    gGoFaster = optionExists("goFaster");
    gDryRun = optionExists("dryRun");
    gInitialLoad = optionExists("initialLoad");
    if (gInitialLoad)
        gGoFaster = TRUE;
    gMaxShrinkage = optionFloat("maxShrinkage", 0.1);
    gAllowLargeDeletes = optionExists("allowLargeDeletes");
    gGbdbGenBank = optionVal("gbdbGenBank", "/gbdb/genbank");
    gNoPerChrom = optionExists("noPerChrom");
    hgSetDb(gDatabase);
    if (gDryRun)
        printf("*** using dry run mode ***\n");
    gbLoadRna(gDatabase, relName, typeStr, limitAccPrefix, reloadList);
    }

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

