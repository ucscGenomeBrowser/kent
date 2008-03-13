/* Load the status table and determine the state of each entry.  Also,
 * various operators base on state. */

#include "common.h"
#include "gbBuildState.h"
#include "dystring.h"
#include "portable.h"
#include "hgRelate.h"
#include "seqTbl.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "gbEntry.h"
#include "sqlDeleter.h"
#include "dbLoadOptions.h"
#include "gbAligned.h"
#include "gbUpdate.h"
#include "gbGenome.h"
#include "gbFileOps.h"
#include "gbProcessed.h"
#include "gbStatusTbl.h"

static char const rcsid[] = "$Id: gbBuildState.c,v 1.25 2008/03/13 00:05:03 markd Exp $";

static struct dbLoadOptions* gOptions; /* options from cmdline and conf */
static int gErrorCnt = 0;  /* count of errors during build */

static boolean loadNonCoding = FALSE;  /* FIXME tmp compile time flag */

struct selectStatusData
/* client data for select status */
{
    struct gbSelect* select;   /* select options */
    struct hash* seqHash;      /* has of seq accession */
    unsigned orgCatDelCnt;     /* count of deleted entries due to orgCat
                                * no longer being selected */
};

static void traceSelect(char* which, struct gbStatus *status)
/* output verbose information when a entry is selected */
{

gbVerbPrStart(5, "%s: %s.%d %s id=%u", which, status->acc, status->version,
              gbFormatDate(status->modDate), status->gbSeqId);
if (status->selectProc != NULL)
    gbVerbPrMore(5, ", proc=%s/%s", status->selectProc->update->name,
                 gbFormatDate(status->selectProc->modDate));
if (status->selectAlign != NULL)
   gbVerbPrMore(5, ", aln=%s/%d", status->selectAlign->update->name,
                status->selectAlign->version);
gbVerbPrMore(5, "\n");
}

static struct gbProcessed* getProcAligned(struct gbEntry* entry,
                                          struct gbAligned** retAligned)
/* Find the processed and aligned entries that should be in the database for
 * an entry.  Returns NULL if none. */
{
struct gbProcessed* processed = NULL;
struct gbAligned* aligned = entry->aligned;

if (aligned != NULL)
    {
    /* Want the newest metadata that has the version that was aligned */
    processed = gbEntryFindProcessed(entry, aligned->version);
    if (processed == NULL)
        errAbort("aligned entry for %s.%d from %s/%s does not have any processed entries",
                 entry->acc, aligned->version,
                 aligned->update->release->name, aligned->update->name);
    }
    
*retAligned = aligned;
return processed;
}

static void flagUpdate(struct gbProcessed* processed,
                       struct gbAligned* aligned,
                       boolean inclAligned)
/* flag an update as needing processed based on a sequence */
{
processed->entry->selectVer = aligned->version;
processed->update->selectProc = TRUE;
if (inclAligned)
    aligned->update->selectAlign = TRUE;
}

static void markDeleted(struct gbStatusTbl* statusTbl,
                        struct gbStatus* tmpStatus,
                        struct selectStatusData* ssData)
/* mark an entry as deleted */
{
struct gbStatus* status = gbStatusStore(statusTbl, tmpStatus);

slAddHead(&statusTbl->deleteList, status);
statusTbl->numDelete++;
status->stateChg = GB_DELETED;
if (!(status->orgCat & ssData->select->orgCats))
    ssData->orgCatDelCnt++;
if (gbVerbose >= 5)
    {
    if (!(status->orgCat & ssData->select->orgCats))
        traceSelect("delete, orgCat not selected", status);
    else
        traceSelect("delete", status);
    }
}

static void markSeqChanged(struct gbStatusTbl* statusTbl,
                           struct gbStatus* tmpStatus,
                           struct gbProcessed* processed,
                           struct gbAligned* aligned)
/* mark an entry as seqChanged; called if version or number of alignments
* changes */
{
struct gbStatus* status = gbStatusStore(statusTbl, tmpStatus);

slAddHead(&statusTbl->seqChgList, status);
statusTbl->numSeqChg++;
status->entry = aligned->entry;
status->selectProc = processed;
status->selectAlign = aligned;
status->stateChg = (GB_SEQ_CHG|GB_META_CHG|GB_EXT_CHG);
status->seqRelease = gbStatusTblGetStr(statusTbl,
                                       aligned->update->release->version);
status->seqUpdate = gbStatusTblGetStr(statusTbl,
                                      aligned->update->shortName);
status->metaRelease = status->seqRelease;
status->metaUpdate = status->seqUpdate;
status->extRelease = status->seqRelease;
status->extUpdate = status->seqUpdate;

flagUpdate(processed, aligned, TRUE);
if (gbVerbose >= 5)
    traceSelect("seqChg", status);
}

static void markMetaChanged(struct gbSelect* select,
                            struct gbStatusTbl* statusTbl,
                            struct gbStatus* tmpStatus,
                            struct gbProcessed* processed,
                            struct gbAligned* aligned)
/* mark an entry as metaChanged */
{
struct gbStatus* status = gbStatusStore(statusTbl, tmpStatus);

slAddHead(&statusTbl->metaChgList, status);
statusTbl->numMetaChg++;
status->entry = processed->entry;
status->selectProc = processed;
status->stateChg = GB_META_CHG;
status->metaRelease = gbStatusTblGetStr(statusTbl,
                                        aligned->update->release->version);
status->metaUpdate = gbStatusTblGetStr(statusTbl,
                                       aligned->update->shortName);
if (!sameString(status->extRelease, select->release->version))
    {
    /* ext seq also changed */
    status->stateChg |= GB_EXT_CHG;
    status->extRelease = status->metaRelease;
    status->extUpdate = status->metaUpdate;
    }

flagUpdate(processed, aligned, FALSE);
if (gbVerbose >= 5)
    traceSelect("metaChg", status);
}

static void markExtChanged(struct gbStatusTbl* statusTbl,
                           struct gbStatus* tmpStatus,
                           struct gbProcessed* processed,
                           struct gbAligned* aligned)
/* mark an entry as extChanged */
{
struct gbStatus* status = gbStatusStore(statusTbl, tmpStatus);

slAddHead(&statusTbl->extChgList, status);
statusTbl->numExtChg++;
status->entry = processed->entry;
status->selectProc = processed;
status->stateChg = GB_EXT_CHG;
status->extRelease = gbStatusTblGetStr(statusTbl,
                                       aligned->update->release->version);
status->extUpdate = gbStatusTblGetStr(statusTbl,
                                       aligned->update->shortName);

/* flag update as needing processing */
flagUpdate(processed, aligned, FALSE);
if (gbVerbose >= 5)
    traceSelect("extChg", status);
}

static void markRebuildDerived(struct gbStatusTbl* statusTbl,
                               struct gbStatus* tmpStatus,
                               struct gbProcessed* processed,
                               struct gbAligned* aligned)
/* mark an entry as rebuilDerived, which is a subset of metaChg */
{
struct gbStatus* status = gbStatusStore(statusTbl, tmpStatus);

slAddHead(&statusTbl->metaChgList, status);  // metaChgList is correct
statusTbl->numRebuildDerived++;
status->entry = processed->entry;
status->selectProc = processed;
status->stateChg = GB_REBUILD_DERIVED;
status->metaRelease = gbStatusTblGetStr(statusTbl,
                                        aligned->update->release->version);
status->metaUpdate = gbStatusTblGetStr(statusTbl,
                                       aligned->update->shortName);
flagUpdate(processed, aligned, FALSE);
#if 0
if (gbVerbose >= 5)
    traceSelect("rebuildDerived", status);
#else
if (gbVerbose >= 1)
    traceSelect("rebuildDerived", status);
#endif
}

static void markNoChange(struct gbStatusTbl* statusTbl,
                         struct gbStatus* tmpStatus,
                         struct gbEntry* entry)
/* mark an entry as noChange; this will not be added to the status table */
{
entry->selectVer = GB_UNCHG_SELECT_VER;
statusTbl->numNoChg++;
if (gbVerbose >= 5)
    traceSelect("noChg", tmpStatus);
}

static void markIgnore(struct gbStatusTbl* statusTbl,
                       struct gbStatus* tmpStatus,
                       struct gbEntry* entry)
/* mark an entry as ignored; this will not be added to the status table */
{
entry->selectVer = GB_UNCHG_SELECT_VER;
if (gbVerbose >= 5)
    traceSelect("ignore", tmpStatus);
}

static void markNew(struct gbStatusTbl* statusTbl,
                    struct gbStatus* status,
                    struct gbProcessed* processed,
                    struct gbAligned* aligned)
/* mark an entry as new */
{
slAddHead(&statusTbl->newList, status);
statusTbl->numNew++;
status->entry = processed->entry;
status->selectProc = processed;
status->selectAlign = aligned;
status->stateChg = GB_NEW;
status->seqRelease = gbStatusTblGetStr(statusTbl,
                                       aligned->update->release->version);
status->seqUpdate = gbStatusTblGetStr(statusTbl,
                                      aligned->update->shortName);
status->metaRelease = status->seqRelease;
status->metaUpdate = status->seqUpdate;
status->extRelease = status->seqRelease;
status->extUpdate = status->seqUpdate;

flagUpdate(processed, aligned, TRUE);
if (gbVerbose >= 5)
    traceSelect("new", status);
}

static void selectStatus(struct gbStatusTbl* statusTbl,
                         struct gbStatus* tmpStatus,
                         void* clientData)
/* Function called to determine if a status entry should be loaded.  This
 * compares the status parsed from the gbStatus file with the gbIndex.
 * Unchanged entries are not loaded into the table, decresing memory required
 * for incremental loads.
 */
{
struct selectStatusData* ssData = clientData;
struct gbEntry* entry = gbReleaseFindEntry(ssData->select->release,
                                           tmpStatus->acc);
struct gbProcessed* processed = NULL;
struct gbAligned* aligned = NULL;
struct hashEl* seqAccEl = hashLookup(ssData->seqHash, tmpStatus->acc);

/* check if in seq table, record if found */
if (seqAccEl == NULL)
    {
    fprintf(stderr, "Error: %s is in gbStatus but not in gbSeq table\n",
            tmpStatus->acc);
    gErrorCnt++;
    }
else
    seqAccEl->val = (void*)TRUE;

if (entry != NULL)
    processed = getProcAligned(entry, &aligned);
/* if no entry or not aligned, or if it shouldn't be included, delete */
if ((entry == NULL) || (aligned == NULL))
    markDeleted(statusTbl, tmpStatus, ssData);
else if (!loadNonCoding && (processed->molType != mol_mRNA))
    markIgnore(statusTbl, tmpStatus, entry);
else
    {
    /* validate entries are not going backwards */
    if (aligned->version < tmpStatus->version)
        errAbort("version for %s in release (%d) is less than one in database (%d)",
                 entry->acc, aligned->version, tmpStatus->version);
    if (processed->modDate < tmpStatus->modDate)
        {
        fprintf(stderr, "Warning: modDate for %s in release (%s) is before one in database (%s)\n",
                entry->acc, gbFormatDate(processed->modDate),
                gbFormatDate(tmpStatus->modDate));
        }
    /* flag updates for changed for latter processing, order of checks is
     * very important.*/
    if ((aligned->version > tmpStatus->version)
        || (aligned->numAligns != tmpStatus->numAligns))
        markSeqChanged(statusTbl, tmpStatus, processed, aligned);
    else if (processed->modDate != tmpStatus->modDate)
        markMetaChanged(ssData->select, statusTbl, tmpStatus, processed,
                        aligned);
    else if (statusTbl->extFileUpdate
             && !sameString(tmpStatus->extRelease,
                            ssData->select->release->version))
        markExtChanged(statusTbl, tmpStatus, processed, aligned);
    else if ((gOptions->flags & DBLOAD_REBUILD_DERIVED)
             && (entry->type == GB_MRNA))
        markRebuildDerived(statusTbl, tmpStatus, processed, aligned);
    else 
        markNoChange(statusTbl, tmpStatus, entry);
    }
}

static void checkNewEntry(struct gbSelect* select, struct gbStatusTbl* statusTbl,
                          struct gbEntry* entry)
/* check if an entry is new */
{
if (entry->selectVer == NULL_VERSION)
    {
    /* new entry, get the alignment.  However if the processed directory
     * has not been aligned yet, it might not exist, in which case, it's
     * ignored.*/
    struct gbAligned* aligned = NULL;
    struct gbProcessed* processed = getProcAligned(entry, &aligned);
    if (!loadNonCoding && (processed != NULL) && (processed->molType != mol_mRNA))
        gbVerbPr(5, "nonCoding: %s.%d %s", entry->acc, entry->processed->version, gbMolTypeSym(processed->molType));
    else if (aligned != NULL)
        {
        struct gbStatus* status
            = gbStatusTblAdd(statusTbl, entry->acc,
                             aligned->version, processed->modDate,
                             entry->type, select->release->srcDb,
                             entry->orgCat, 0, 0,
                             aligned->update->release->version,
                             aligned->update->shortName, 0);
        markNew(statusTbl, status, processed, aligned);
        }
    else if (gbVerbose >= 5)
        {
        gbVerbPr(5, "notAligned: %s.%d %s", entry->acc, entry->processed->version,
                 gbFormatDate(entry->processed->modDate));
        }
    }
}

static void findNewEntries(struct gbSelect* select, struct gbStatusTbl* statusTbl)
/* Traverse the gbIndex file to find entries that were not in the gbStatus
 * table when loaded.  These are new entries. */
{
struct hashCookie cookie;
struct hashEl* hel;
cookie = hashFirst(select->release->entryTbl);
while ((hel = hashNext(&cookie)) != NULL)
    checkNewEntry(select, statusTbl, (struct gbEntry*)hel->val);
}

static int flagOrphans(struct sqlConnection *conn, struct gbSelect* select,
                       struct hash *seqHash, struct gbStatusTbl* statusTbl)
/* find orphans in gbSeq table that were not found in gbStatus and flag them
 *in the status table */
{
struct hashCookie cookie;
struct hashEl* hel;
int numOrphans = 0;

/* Find orphans using seqHash.  This was previously implemented using
 *     SELECT seq.acc FROM seq LEFT JOIN gbStatus ON (seq.acc = gbStatus.acc)
 *      WHERE (seq.type = '%s') AND (seq.srcDb = '%s')
 *      AND (gbStatus.seq IS NULL);
 * however this proved many times slower than reading the sequences into
 * a hash.
 */

cookie = hashFirst(seqHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (!hel->val)
        {
        struct gbStatus *status = gbStatusTblFind(statusTbl, hel->name);
        if (status != NULL)
            {
            status->stateChg |= GB_ORPHAN;
            if (!(status->stateChg & GB_NEW))
                errAbort("oprhaned seq entry for %s is not flaged as new",
                         status->acc);
            numOrphans++;
            }
        else
            {
            /* Orphan, but not in table. This could be an entry that was
             * deleted from gbStatus table by hand. Leave versions empty */
            fprintf(stderr, "Warning: %s is orphaned, but not a new entry, deleting\n",
                    hel->name);
            status = gbStatusTblAdd(statusTbl, hel->name,  0, 0, select->type,
                                    select->release->srcDb, 0, 0, 0,
                                    "", "", 0);
            slAddHead(&statusTbl->deleteList, status);
            statusTbl->numDelete++;
            status->stateChg = GB_DELETED;
            }
        }
    }
return numOrphans;
}

static void makeOrphanList(struct gbStatusTbl* statusTbl)
/* move orphans from new to orphan list */
{
struct gbStatus* stat;
struct gbStatus *next = statusTbl->newList;
statusTbl->newList = NULL;
statusTbl->numNew = 0;
while ((stat = next) != NULL)
    {
    next = stat->next;
    if (stat->stateChg & GB_ORPHAN)
        {
        slAddHead(&statusTbl->orphanList, stat);
        statusTbl->numOrphan++;
        }
    else
        {
        slAddHead(&statusTbl->newList, stat);
        statusTbl->numNew++;
        }
    }
}

static void findOrphans(struct sqlConnection *conn, struct gbSelect* select,
                        struct hash *seqHash, struct gbStatusTbl* statusTbl)
/* Find orphans, these are entries that are entries that are in the seq table
 * but not in the gbStatus, which would have results from a failed load.
 * If they are new, it's from a failed load.  Hand-deleting from the
 * gbStatus table will also result in orphans (and reloading them).
 */
{
int numOrphans = flagOrphans(conn, select, seqHash, statusTbl);
if (numOrphans > 0)
    makeOrphanList(statusTbl);
}

static bool checkForAccTypeChange(struct sqlConnection *conn, 
                                  struct gbSelect* select,
                                  struct gbStatus* status)
/* Check if a sequence that appears new has really had it's type has changed.
 * Returns true if type changed (or other error), false if nothing detected.
 */
{
char query[128];
struct sqlResult* sr;
char **row;
bool changed = FALSE;

safef(query, sizeof(query),
      "SELECT type FROM gbSeq WHERE acc = '%s'", status->acc);
sr = sqlGetResult(conn, query);
if ((sr != NULL) && ((row = sqlNextRow(sr)) != NULL))
    {
    unsigned type = gbParseType(row[0]);
    if (type != status->type)
        fprintf(stderr,
                "Error: %s %s type has changed from %s to %s; add to ignore file\n",
                status->acc, gbFormatDate(status->modDate),
                gbFmtSelect(type), gbFmtSelect(status->type));
    else
        fprintf(stderr,
                "Error: %s %s is in the seq table, but shouldn't be, don't know why\n",
                status->acc, gbFormatDate(status->modDate));
    changed = TRUE;
    gErrorCnt++;
    }
sqlFreeResult(&sr);
return changed;
}

static void checkForTypeChange(struct sqlConnection *conn, 
                               struct gbSelect* select,
                               struct gbStatusTbl* statusTbl)
/* Check that new sequences are not already in the seq table.  This handles
 * the case of a sequence's type changing.  Of course, this  isn't a real
 * change, but a correction to the genbank record.  Due to the partationing
 * between mRNAs and EST, this isn't detected normally.  But it's a pain
 * to find when it happens, as it just results in a tab file load failure.
 * This shouldn't be called for initialLoad, as it would be very slow and
 * not accomplish anything since the tables are loaded at the end.
 */
{
if (sqlTableExists(conn, SEQ_TBL))
    {
    int changeCnt = 0;
    struct gbStatus* status;
    for (status = statusTbl->newList; status != NULL; status = status->next)
        {
        if (checkForAccTypeChange(conn, select, status))
            changeCnt++;
        }
    if (changeCnt > 0)
        errAbort("%d accession types have changed, add incorrect ones to data/ignore.idx",
                 changeCnt);
    }
}

static void listDeletedAcc(struct gbSelect* select, struct gbStatusTbl* statusTbl)
/* print the accessions being deleted */
{
struct gbStatus *status = statusTbl->deleteList;
printf("deleted accessions for %s\n", gbSelectDesc(select));
for (; status != NULL; status = status->next)
    printf("\t%s\n", status->acc);
}

static boolean checkShrinkage(struct gbSelect* select, float maxShrinkage,
                              struct gbStatusTbl* statusTbl)
/* Check for too much shrinkage, print deleted if exeeeded and return
 * FALSE.  Return true if ok.*/
{
float shrinkage = 0.0;
unsigned numOld = statusTbl->numDelete + statusTbl->numSeqChg
    + statusTbl->numMetaChg + statusTbl->numRebuildDerived + statusTbl->numExtChg + statusTbl->numNoChg;
unsigned numNew  = statusTbl->numSeqChg + statusTbl->numMetaChg
    + statusTbl->numExtChg + +statusTbl->numRebuildDerived + statusTbl->numNoChg + statusTbl->numNew;
if (numNew < numOld)
    {
    /* FIXME: the at least 5 feels like a hack */
    shrinkage = 1.0 - ((float)numNew/(float)numOld);
    if ((maxShrinkage > 0) && ((numOld-numNew) < 5))
        shrinkage = 0;  /* allow for small partations */
    if (shrinkage > maxShrinkage)
        {
        fprintf(stderr, 
                "Error: size after deletion exceeds maximum shrinkage for %s,\n"
                "Rerun with -allowLargeDeletes to overrided.\n"
                "Will continue checking for other large deletes.\n"
                 "delete=%u seqChg=%u metaChg=%u extChg=%u new=%u orphan=%u derived=%u noChg=%u\n",
                 gbSelectDesc(select), statusTbl->numDelete,
                statusTbl->numSeqChg, statusTbl->numMetaChg,
                statusTbl->numExtChg, statusTbl->numNew, statusTbl->numOrphan,
                statusTbl->numRebuildDerived, statusTbl->numNoChg);
        listDeletedAcc(select, statusTbl);
        return FALSE;
        }
    }
return TRUE;
}

struct gbStatusTbl* gbBuildState(struct sqlConnection *conn,
                                 struct gbSelect* select, 
                                 struct dbLoadOptions* options,
                                 float maxShrinkage,
                                 char* tmpDir,
                                 int verboseLevel,
                                 boolean extFileUpdate,
                                 boolean* maxShrinkageExceeded)
/* Load status table and find of state of all genbank entries in the release
 * compared to the database. */
{
struct gbStatusTbl* statusTbl;
struct selectStatusData ssData;
unsigned selectFlags = (select->type | select->release->srcDb);
ZeroVar(&ssData);

gOptions = options;
*maxShrinkageExceeded = FALSE;
gbVerbose = verboseLevel;
gErrorCnt = 0;

gbVerbEnter(3, "build state table");
gbVerbMsg(4, "reading gbSeq accessions");
ssData.select = select;
ssData.seqHash = seqTblLoadAcc(conn, select);

gbVerbMsg(4, "reading gbStatus");
statusTbl = gbStatusTblSelectLoad(conn, selectFlags, select->accPrefix,
                                  selectStatus, &ssData,
                                  tmpDir, extFileUpdate, (gbVerbose >= 4));
findNewEntries(select, statusTbl);

/* Don't allow deletes when select criteria has changed */
if ((ssData.orgCatDelCnt > 0) && !(gOptions->flags & DBLOAD_LARGE_DELETES))
    errAbort("%u entries deleted due to organism category no longer being selected, specify -allowLargeDeletes to override",
             ssData.orgCatDelCnt);

/* check shrinkage unless override */
if ((gOptions->flags & DBLOAD_LARGE_DELETES) == 0)
    {
    if (!checkShrinkage(select, maxShrinkage, statusTbl))
        *maxShrinkageExceeded = TRUE;
    }

/* don't do other setup if we are going to stop on maxShrinkageExceeded */
if (!*maxShrinkageExceeded)
    {
    gbVerbMsg(4, "checking for orphans");
    findOrphans(conn, select, ssData.seqHash, statusTbl);

    if (((gOptions->flags & DBLOAD_INITIAL) == 0))
        {
        gbVerbMsg(4, "checking for type change");
        checkForTypeChange(conn, select, statusTbl);
        }
    }

hashFree(&ssData.seqHash);

gbVerbLeave(3, "build state table");

/* always print stats */
fprintf(stderr, "gbLoadRna: selected %s: delete=%u seqChg=%u metaChg=%u extChg=%u new=%u orphan=%u derived=%u noChg=%u\n",
        gbSelectDesc(select), statusTbl->numDelete, statusTbl->numSeqChg,
        statusTbl->numMetaChg, statusTbl->numExtChg, statusTbl->numNew,
        statusTbl->numOrphan, statusTbl->numRebuildDerived, 
        statusTbl->numNoChg);

/* this doesn't include large delete errors */
if (gErrorCnt > 0)
    errAbort("Errors detecting when constructing state table");
return statusTbl;
}

struct sqlDeleter *gbBuildStateReloadDeleter(struct sqlConnection *conn,
                                             struct gbSelect* select,
                                             char *tmpDirPath)
/* get deleter for list of accessions to reload for the selected categories.
 * Used when reloading. Returns null if none found */
{
struct hash* seqTblAccs;
struct slName *gbStatAccs, *acc;
struct hashCookie cookie;
struct hashEl *hel;
struct sqlDeleter* deleter;
unsigned cnt = 0;
unsigned statTblSelect = select->release->srcDb | select->orgCats | select->type;

/* ESTs not implemented, which gets rid of complexities of accPrefix */
if (select->type & GB_EST)
    errAbort("gbBuildStateReloadDeleter doesn't handle ESTs");

seqTblAccs = seqTblLoadAcc(conn, select);
gbStatAccs = gbStatusTblLoadAcc(conn,  statTblSelect,  NULL);

/* build up deleter combining the two */
deleter = sqlDeleterNew(tmpDirPath, (gbVerbose >= 4));

cookie = hashFirst(seqTblAccs);
while ((hel = hashNext(&cookie)) != NULL)
    {
    sqlDeleterAddAcc(deleter, hel->name);
    cnt++;
    }

for (acc = gbStatAccs; acc != NULL; acc = acc->next)
    {
    if (hashLookup(seqTblAccs, acc->name) == NULL)
        {
        sqlDeleterAddAcc(deleter, acc->name);
        cnt++;
        }
    }

hashFree(&seqTblAccs);
slFreeList(&gbStatAccs);
if (cnt == 0)
    sqlDeleterFree(&deleter);

return deleter;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

