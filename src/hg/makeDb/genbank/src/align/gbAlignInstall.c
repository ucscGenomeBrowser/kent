#include "gbAlignCommon.h"
#include "gbIndex.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbAligned.h"
#include "gbFileOps.h"
#include "gbPipeline.h"
#include "gbVerb.h"
#include "gbIgnore.h"
#include "common.h"
#include "portable.h"
#include "options.h"
#include "psl.h"
#include "estOrientInfo.h"
#include <stdio.h>


/*
 * Notes:
 *  - sorting into chromosome order is done here so that migrated alignments
 *    will also be sorted.
 *  - A lot more bookkeeping counts are collected than actually used or verified.
 *    This could be simplified, but it might be helpful to print the info for
 *    debugging.  The count of alignments has found several subtle bugs.
 */

/* FIXME: a wrapper around FILE that counts records written would simplify
 * some of this */

/* Type of PSL file */
#define MAIN_PSL_FILE    0
#define RAW_PSL_FILE     1
#define INTRON_PSL_FILE  2

/* table, keyed by above const, of gziped psl file extensions */
static char* gPslFileGzExt[] = {
    "psl.gz", "rawPsl.gz", "intronPsl.gz"
};

/* table, keyed by above constion, of psl file extension without .gz;
 * also servers as a description. */
static char* gPslFileExt[] = {
    "psl", "rawPsl", "intronPsl"
};


/* PSL sort spec: tName tStart tEnd qName qStart qEnd */
static char *PSL_SORT_SPEC[] = {
    "--key=14,14", "--key=16n,16n", "--key=17n,17n",
    "--key=10,10", "--key=12n,12n", "--key=13n,13n",
    NULL
};

/* orientation info sort spec: chrom start end name */
static char *OI_SORT_SPEC[] = {
    "--key=1,1", "--key=2n,2n", "--key=3n,3n", "--key=4,4",
    NULL
};

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"workdir", OPTION_STRING},
    {"orgCats", OPTION_STRING},
    {"noMigrate", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {NULL, 0}
};


/* global parameters from command line */
static char* gWorkDir;
static char* gSortTmp;

struct recCounts
/* Counts for each type of table record */
{
    struct gbEntryCnts pslCnts;       /* PSLs */
    struct gbEntryCnts rawPslCnts;    /* raw PSLs */
    struct gbEntryCnts oiCnts;        /* OIs */
    struct gbEntryCnts intronPslCnts; /* intron PSLs */
};

struct migrateAligns
/* Object to collect info about alignments to migrated.  This is a little
 * tricky, in that alignment records are migrated even if the accession didn't
 * align. */
{
    struct gbSelect* select;          /* selection criteria */
    struct gbSelect* prevSelect;      /* previous release info */
    struct recCounts counts;          /* Counts of various records */
};

struct outputFile
/* with output file path and FILE */
{
    char path[PATH_LEN];
    FILE *fh;
};

struct outputFiles
/* structure to pass around output files */
{
    struct outputFile psl;         /* psl output file */
    struct outputFile rawPsl;      /* raw psl output file */
    struct outputFile oi;          /* orient info output file */
    struct outputFile intronPsl;   /* intronPsl output file */
};

void recCountsSum(struct recCounts* accum, struct recCounts* counts)
/* sum recCounts objects */
{
gbEntryCntsSum(&accum->pslCnts, &counts->pslCnts);
gbEntryCntsSum(&accum->oiCnts, &counts->oiCnts);
gbEntryCntsSum(&accum->intronPslCnts, &counts->intronPslCnts);
}

FILE *openSortOutput(char *fileName, char **sortSpec)
/* Open a pipeline to sort and compress output.  SortSpec is null-terminated
 * list of fields options to pass to sort.  gSortTmpDir maybe null.
 * gbOutputRename is used to install and close.
 */
{
struct gbPipeline *pipeline;
char tmpPath[PATH_LEN];
char **compressor;
char **sortCmd, **cmds[3];
int nSort, iSort = 0, i;

/* sort cmd, nSort includes cmd and NULL */
for (nSort = 0; sortSpec[nSort] != NULL; nSort++)
    continue;
if (gSortTmp != NULL)
    nSort += 2;
nSort += 2; /* for "sort" and  NULL */
sortCmd = alloca(nSort*sizeof(char*));
sortCmd[iSort++] = "sort";
if (gSortTmp != NULL)
    {
    sortCmd[iSort++] = "-T";
    sortCmd[iSort++] = gSortTmp;
    }
for (i = 0; sortSpec[i] != NULL; i++)
    sortCmd[iSort++] = sortSpec[i];
sortCmd[iSort] = NULL;
assert(iSort < nSort);
cmds[0] = sortCmd;
cmds[1] = NULL;

/* compress process, if needed */
compressor = gbGetCompressor(fileName, "w");
if (compressor != NULL)
    {
    cmds[1] = compressor;
    cmds[2] = NULL;
    }

/* tmp name to output to */
gbGetOutputTmp(fileName, tmpPath);
gbMakeFileDirs(tmpPath);

pipeline = gbPipelineCreate(cmds, PIPELINE_WRITE|PIPELINE_FDOPEN, NULL,
                            tmpPath);
return gbPipelineFile(pipeline);
}

struct gbEntry* getEntry(struct gbSelect* select, char* acc, char* refFile)
/* Get the entry obj for an accession referenced in a file.  If not found and
 * accession ignored, return NULL, otherwise it's and error.  This allows for
 * an ignored entry to be added after alignment.  Also check that the entry is
 * part of the current update.  This handles this an ignored accession being
 * found in a PSL, etc.
 */
{
struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
if (entry == NULL)
    {
    /* note: this isn't actually checking for a specific acc/moddate being
     * ignored, it's just not generating an error if the entry is not found
     * and it's ignored for any moddate. */
    if (gbIgnoreFind(select->release->ignore, acc) != NULL)
        return NULL;  // igored, assume the best
    errAbort("can't find accession \"%s\" in gbIndex, referenced in %s",
             acc, refFile);
    }

/* check for being in this update */
assert(select->update != NULL);
if (gbEntryFindUpdateProcessed(entry, select->update) == NULL)
    {
    assert(gbIgnoreFind(select->release->ignore, acc) != NULL);
    return NULL;  /* not in update, probably ignored */
    }
return entry;
}

struct gbAligned* getMigrateAligned(struct migrateAligns* migrate,
                                    char* accVer, char* refFile)
/* check if an acc is select for migration, and if so return it's
 * new gbAligned object. */
{
char acc[GB_ACC_BUFSZ];
unsigned version = gbSplitAccVer(accVer, acc);
struct gbEntry* prevEntry = getEntry(migrate->prevSelect, acc, refFile);

/* Check that this a select acc of the right version */
if ((prevEntry != NULL)
    && (prevEntry->clientFlags & MIGRATE_FLAG)
    && (version == prevEntry->selectVer))
    {
    /* need to check that this is the update for the first aligned entry
     * with this version.  This handled a case were the same version
     * was aligned in multiple updates.  This normally shouldn't happend. */
    struct gbAligned* prevAligned = gbEntryFindAlignedVer(prevEntry, version);
    if (prevAligned->update == migrate->prevSelect->update)
        {
        /* ok, not get the current entry.  Only null if igored */
        struct gbEntry* entry = getEntry(migrate->select, acc, refFile);
        if (entry != NULL)
            return gbEntryGetAligned(entry, migrate->select->update, version, NULL);
        }
    }
return NULL;
}

void migratePsl(struct migrateAligns* migrate, unsigned pslFileType,
                struct gbEntryCnts* counts, struct psl* psl,
                char* inPsl, FILE* outPslFh)
/* Migrate PSL, if it's accession and version are flagged */
{
struct gbAligned* aligned = getMigrateAligned(migrate, psl->qName, inPsl);
if (aligned != NULL)
    {
    pslTabOut(psl, outPslFh);

    if (pslFileType == MAIN_PSL_FILE)
        {
        /* count main psls in index. */
        gbAlignedCount(aligned, 1);
        /* increment accession count if this is the first one */
        gbCountNeedAligned(counts, aligned->entry,
                           ((aligned->numAligns == 1) ? 1 : 0), 1);
        }
    else
        {
        /* for rawPsl and intronPsl only count PSLs */
        gbCountNeedAligned(counts, aligned->entry, 0, 1);
        }
    if (gbVerbose >= 3)
        gbVerbPr(3, "migrating %s %s %s",
                 gbOrgCatName(aligned->entry->orgCat), 
                 gPslFileExt[pslFileType], psl->qName);
    }
}

void migratePsls(struct migrateAligns* migrate, unsigned pslFileType,
                 struct gbEntryCnts* counts, FILE* outPslFh)
/* Migrate selected PSL records */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;

gbAlignedGetPath(migrate->prevSelect, gPslFileGzExt[pslFileType], NULL, inPsl);

/* It's possible to end up here and not have a file if none of the sequences
 * aligned */
if (fileExists(inPsl))
    {
    gbVerbEnter(2, "migrating %ss from %s", gPslFileExt[pslFileType], inPsl);
    inPslLf = gzLineFileOpen(inPsl);
    while ((psl = pslNext(inPslLf)) != NULL)
        {
        migratePsl(migrate, pslFileType, counts, psl, inPsl, outPslFh);
        pslFree(&psl);
        }
    gzLineFileClose(&inPslLf);
    gbVerbLeave(2, "migrating %ss from %s", gPslFileExt[pslFileType], inPsl);
    }
}

void migrateOrientInfo(struct migrateAligns* migrate,
                       struct estOrientInfo* oi, char* inOi, FILE* outOiFh)
/* Migrate a estOrientInfo row, if it's accession and version are flagged */
{
struct gbAligned* aligned = getMigrateAligned(migrate, oi->name, inOi);
if (aligned != NULL)
    {
    if (gbVerbose >= 3)
        gbVerbPr(3, "migrating %s oi %s",
                 gbOrgCatName(aligned->entry->orgCat), oi->name);
    estOrientInfoTabOut(oi, outOiFh);
    /* just count records */
    gbCountNeedAligned(&migrate->counts.oiCnts, aligned->entry, 0, 1);
    }
}

void migrateOrientInfos(struct migrateAligns* migrate, FILE* outOiFh)
/* Migrate estOrientInfo records */
{
char inOi[PATH_LEN];
struct lineFile* inOiLf;
char *row[EST_ORIENT_INFO_NUM_COLS];

gbAlignedGetPath(migrate->prevSelect, "oi.gz", NULL, inOi);

if (fileExists(inOi))
    {
    gbVerbEnter(2, "migrating from %s", inOi);
    inOiLf = gzLineFileOpen(inOi);
    while (lineFileNextRowTab(inOiLf, row, ArraySize(row)))
        {
        struct estOrientInfo *oi = estOrientInfoLoad(row);
        migrateOrientInfo(migrate, oi, inOi, outOiFh);
        estOrientInfoFree(&oi);
        }
    gzLineFileClose(&inOiLf);
    gbVerbLeave(2, "migrating from %s", inOi);
    }
}

void migrateAlignedUpdate(struct gbSelect* prevSelect,
                          struct migrateAligns* migrate,
                          struct outputFiles* out,
                          struct recCounts* recCounts)
/* Migrate existing aligned PSLs from an earlier release update. */
{
/* process on if entries to migrate for this orgCat */
if (prevSelect->update->selectAlign & prevSelect->orgCats)
    {
    migratePsls(migrate, MAIN_PSL_FILE, &migrate->counts.pslCnts,
                out->psl.fh);
    if (out->rawPsl.fh != NULL)
        migratePsls(migrate, RAW_PSL_FILE, &migrate->counts.rawPslCnts,
                    out->rawPsl.fh);
    if (out->oi.fh != NULL)
        migrateOrientInfos(migrate, out->oi.fh);
    if (out->intronPsl.fh != NULL)
        migratePsls(migrate, INTRON_PSL_FILE, &migrate->counts.intronPslCnts,
                    out->intronPsl.fh);
    }
}

void migrateAligned(struct gbSelect* select, struct gbSelect* prevSelect,
                    struct gbAlignInfo* alignInfo, struct outputFiles* out,
                    struct recCounts* recCounts)
/* Migrate existing aligned PSLs from an earlier release. */
{
int orgCatIdx = gbOrgCatIdx(select->orgCats);
struct gbUpdate* prevUpdateHold = prevSelect->update;
struct gbUpdate* prevUpdate;
struct migrateAligns migrate;
ZeroVar(&migrate);
migrate.select = select;
migrate.prevSelect = prevSelect;

/* traverse all updates in the previous release */
gbVerbEnter(1, "migrating alignments");
for (prevUpdate = prevSelect->release->updates; prevUpdate != NULL;
     prevUpdate = prevUpdate->next)
    {
    prevSelect->update = prevUpdate;
    migrateAlignedUpdate(prevSelect, &migrate, out, recCounts);
    }
prevSelect->update = prevUpdateHold;
recCountsSum(recCounts, &migrate.counts);
if (migrate.counts.pslCnts.recCnt[orgCatIdx] != alignInfo->migrate.recCnt[orgCatIdx])
    errAbort("expected to migrate %d %s PSLs, found %d",
             alignInfo->migrate.recCnt[orgCatIdx], gbOrgCatName(select->orgCats),
             migrate.counts.pslCnts.recCnt[orgCatIdx]);
gbVerbLeave(1, "migrating alignments");
}

void copyPsl(struct gbSelect* select, unsigned pslFileType,
             struct psl* psl, char* inPsl,
             FILE* outPslFh, struct gbEntryCnts* counts)
/* Copy a PSL. */
{
char acc[GB_ACC_BUFSZ];
int version = gbSplitAccVer(psl->qName, acc);
struct gbAligned* aligned;
struct gbEntry* entry = getEntry(select, acc, inPsl);
if (entry == NULL)
    errAbort("no entry for %s %s in %s", gPslFileExt[pslFileType],
             psl->qName, inPsl);
aligned = gbEntryGetAligned(entry, select->update, version, NULL);
pslTabOut(psl, outPslFh);
if (pslFileType == MAIN_PSL_FILE)
    {
    /* count main psls in index. */
    gbAlignedCount(aligned, 1);
    /* increment accession count if this is * the first one */
    gbCountNeedAligned(counts, entry,
                           ((aligned->numAligns == 1) ? 1 : 0), 1);
    }
else
    {
    /* for rawPsl and intronPsl only count PSLs */
    gbCountNeedAligned(counts, entry, 0, 1);
    }
if (gbVerbose >= 3)
    gbVerbPr(3, "installing %s %s %s.%d", gbOrgCatName(entry->orgCat),
             gPslFileExt[pslFileType], acc, version);
}

void copyPsls(struct gbSelect* select, unsigned pslFileType, FILE* outPslFh,
              struct gbEntryCnts* counts)
/* Copy a PSL file from the work directory if it exists, count alignments
 * for index. */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;

gbAlignedGetPath(select, gPslFileExt[pslFileType], gWorkDir, inPsl);
if (fileExists(inPsl))
    {
    gbVerbEnter(2, "installing from %s", inPsl);
    inPslLf = gzLineFileOpen(inPsl);
    while ((psl = pslNext(inPslLf)) != NULL)
        {
        copyPsl(select, pslFileType, psl, inPsl, outPslFh, counts);
        pslFree(&psl);
        }
    gzLineFileClose(&inPslLf);
    gbVerbLeave(2, "installing from %s", inPsl);
    }
}

void copyOrientInfo(struct gbSelect* select, struct estOrientInfo* oi,
                    char* inOi, FILE* outOiFh, struct recCounts* recCounts)
/* Copy a orientInfo record. */
{
char acc[GB_ACC_BUFSZ];
struct gbEntry* entry;
gbSplitAccVer(oi->name, acc);
entry = getEntry(select, acc, inOi);
if (entry != NULL)
    {
    if (gbVerbose >= 3)
        gbVerbPr(3, "installing %s oi %s", gbOrgCatName(entry->orgCat),
                 oi->name);
    estOrientInfoTabOut(oi, outOiFh);
    /* just count records */
    gbCountNeedAligned(&recCounts->oiCnts, entry, 0, 1);
    }
}

void copyOrientInfos(struct gbSelect* select, FILE* outOiFh,
                     struct recCounts* recCounts)
/* Copy an OI file from the work directory, if it exists, count alignments
 * for index. */
{
char inOi[PATH_LEN];
struct lineFile* inOiLf;
char *row[EST_ORIENT_INFO_NUM_COLS];

gbAlignedGetPath(select, "oi", gWorkDir, inOi);
if (fileExists(inOi))
    {
    gbVerbEnter(2, "installing from %s", inOi);
    inOiLf = gzLineFileOpen(inOi);
    while (lineFileNextRowTab(inOiLf, row, ArraySize(row)))
        {
        struct estOrientInfo *oi = estOrientInfoLoad(row);
        copyOrientInfo(select, oi, inOi, outOiFh, recCounts);
        estOrientInfoFree(&oi);
        }
    gzLineFileClose(&inOiLf);
    gbVerbLeave(2, "installing from %s", inOi);
    }
}

void copyIntronPsl(struct gbSelect* select, struct psl* psl, char* inPsl,
                   FILE* outPslFh, struct recCounts* recCounts)
/* Copy an intronPsl. */
{
char acc[GB_ACC_BUFSZ];
struct gbEntry* entry;
gbSplitAccVer(psl->qName, acc);
entry = getEntry(select, acc, inPsl);
if (entry != NULL)
    {
    if (gbVerbose >= 3)
        gbVerbPr(3, "installing %s intronPsl %s", gbOrgCatName(entry->orgCat),
                 psl->qName);
    pslTabOut(psl, outPslFh);
    /* just count records */
    gbCountNeedAligned(&recCounts->intronPslCnts, entry, 0, 1);
    }
}

void copyIntronPsls(struct gbSelect* select, FILE* outPslFh,
                    struct recCounts* recCounts)
/* Copy an intron PSL file from the work directory if it exists */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;

gbAlignedGetPath(select, "intronPsl", gWorkDir, inPsl);
if (fileExists(inPsl))
    {
    gbVerbEnter(2, "installing from %s", inPsl);
    inPslLf = gzLineFileOpen(inPsl);
    while ((psl = pslNext(inPslLf)) != NULL)
        {
        copyIntronPsl(select, psl, inPsl, outPslFh, recCounts);
        pslFree(&psl);
        }
    gzLineFileClose(&inPslLf);
    gbVerbLeave(2, "installing from %s", inPsl);
    }
}

void createAlignedIndex(struct gbSelect* select, char* alignIdx)
/* create an alignment index from the alignRecs stored in the index.
 * it is not renamed from the tmp file name here, just closed
 */
{
struct gbProcessed* processed;
FILE *alignIdxFh;

/* setup output PSL files */
gbAlignedGetPath(select, "alidx", NULL, alignIdx);
alignIdxFh = gbMustOpenOutput(alignIdx);

/* visit all processed entries for this update  */
for (processed = select->update->processed; processed != NULL;
     processed = processed->updateLink)
    {
    struct gbEntry* entry = processed->entry;
    if ((entry->clientFlags & (MIGRATE_FLAG|ALIGN_FLAG))
         && (entry->orgCat & select->orgCats))
        {
        struct gbAligned* aligned = gbEntryFindAlignedVer(entry, processed->version);
        int numAligns = ((aligned != NULL) ? aligned->numAligns : 0);
        gbAlignedWriteIdxRec(alignIdxFh, entry->acc, processed->version, numAligns);
        }
    }

carefulClose(&alignIdxFh);
}

void installOrgCatAligned(struct gbSelect* select, unsigned orgCat,
                          struct gbSelect* prevSelect,
                          struct gbAlignInfo* alignInfo,
                          char* alignIdx)
/* Install alignments for either native or xeno.  The alignment index is
 * created and named returned, but not renamed until both native and xeno are
 * processed. */
{
unsigned holdOrgCats = select->orgCats;
struct outputFiles out;
struct recCounts recCounts;
ZeroVar(&out);
ZeroVar(&recCounts);

select->orgCats = orgCat;
if (prevSelect != NULL)
    prevSelect->orgCats = orgCat;

/* setup out PSL and orientInfo files */
gbAlignedGetPath(select, "psl.gz", NULL, out.psl.path);
out.psl.fh = openSortOutput(out.psl.path, PSL_SORT_SPEC);
if (select->orgCats == GB_NATIVE)
    {
    gbAlignedGetPath(select, "oi.gz", NULL, out.oi.path);
    out.oi.fh = openSortOutput(out.oi.path, OI_SORT_SPEC);
    if (select->type == GB_EST)
        {
        gbAlignedGetPath(select, "intronPsl.gz", NULL, out.intronPsl.path);
        out.intronPsl.fh = openSortOutput(out.intronPsl.path, PSL_SORT_SPEC);
        }
    }
if (select->type == GB_MRNA)
    {
    /* we don't bother sorting raw psl */
    gbAlignedGetPath(select, "rawPsl.gz", NULL, out.rawPsl.path);
    out.rawPsl.fh = gbMustOpenOutput(out.rawPsl.path);
    }

/* previous aligned if this is a full update */
if (prevSelect != NULL)
    migrateAligned(select, prevSelect, alignInfo, &out, &recCounts);

/* copy currently aligned, if they exist */
copyPsls(select, MAIN_PSL_FILE, out.psl.fh, &recCounts.pslCnts);
if (select->type == GB_MRNA)
    copyPsls(select, RAW_PSL_FILE, out.rawPsl.fh, &recCounts.rawPslCnts);
if ((select->orgCats == GB_NATIVE) && (recCounts.pslCnts.recTotalCnt > 0))
    {
    /* copy new OI and intronPsls */
    copyOrientInfos(select, out.oi.fh, &recCounts);
    if (select->type == GB_EST)
        copyPsls(select, INTRON_PSL_FILE, out.intronPsl.fh,
                 &recCounts.intronPslCnts);
    }

/* Install or remove files.  Done seperate from copy due to posibility of
* all being migrated*/
if (recCounts.intronPslCnts.recTotalCnt > 0)
    gbOutputRename(out.intronPsl.path, &out.intronPsl.fh);
else
    gbOutputRemove(out.intronPsl.path, &out.intronPsl.fh);

if (recCounts.oiCnts.recTotalCnt > 0)
    gbOutputRename(out.oi.path, &out.oi.fh);
else
    gbOutputRemove(out.oi.path, &out.oi.fh);

if (recCounts.rawPslCnts.recTotalCnt > 0)
    gbOutputRename(out.rawPsl.path, &out.rawPsl.fh);
else
    gbOutputRemove(out.rawPsl.path, &out.rawPsl.fh);

if (recCounts.pslCnts.recTotalCnt > 0)
    gbOutputRename(out.psl.path, &out.psl.fh);
else
    gbOutputRemove(out.psl.path, &out.psl.fh);

createAlignedIndex(select, alignIdx);

select->orgCats = holdOrgCats;
if (prevSelect != NULL)
    prevSelect->orgCats = holdOrgCats;
}

void gbAlignInstall(struct gbSelect* select, struct gbSelect* prevSelect)
/* Install alignments, optionally migrating unchanged ones from a previous
 * release.  This does one update, accPrefix and either native or xeno */
{
char nativeAlignIdx[PATH_LEN], xenoAlignIdx[PATH_LEN];
struct gbAlignInfo alignInfo;

gbVerbEnter(1, "gbAlignInstall: %s", gbSelectDesc(select));

/* load required entry date */
gbReleaseLoadProcessed(select);
if (prevSelect != NULL)
    {
    gbReleaseLoadProcessed(prevSelect);
    gbReleaseLoadAligned(prevSelect);
    }

/* mark entries and updates to migrate or align */
alignInfo = gbAlignFindNeedAligned(select, prevSelect);

/* Process each category */
if (select->orgCats & GB_NATIVE)
    installOrgCatAligned(select, GB_NATIVE, prevSelect, &alignInfo,
                         nativeAlignIdx);
if (select->orgCats & GB_XENO)
    installOrgCatAligned(select, GB_XENO, prevSelect, &alignInfo,
                         xenoAlignIdx);

/* now indices can be renamed, not completely atomic, but good enough */
if (select->orgCats & GB_NATIVE)
    gbOutputRename(nativeAlignIdx, NULL);
if (select->orgCats & GB_XENO)
    gbOutputRename(xenoAlignIdx, NULL);

/* print message before memory is freed */
gbVerbLeave(1, "gbAlignInstall: %s", gbSelectDesc(select));

/* unload entries to free memory */
gbReleaseUnload(select->release);
if (prevSelect != NULL)
    gbReleaseUnload(prevSelect->release);
}

void usage()
/* print usage and exit */
{
errAbort("   gbAlignInstall relname update typeAccPrefix db\n"
         "\n"
         "Install aligned sequences that were obtained with gbAlignGet\n"
         "and aligned. This also creates a index files which include\n"
         "sequences that did not align.  If the full release is being\n"
         "processed and there is a previous release, the existing PSL\n"
         "alignments will be migrate and copied with the new PSLs\n"
         "Note that the full update should be the first processed in a\n"
         "release for migration to work.  The input PSL file do not need\n"
         "to be sorted by chromosome location, as this is done here.\n"
         "This looks for the combined and lifted alignment file in the\n"
         "work directory.  This is asymetrical with gbAlignGet as only one\n"
         "output PSL is processed by a call to this program as this was \n"
         "easier to script.\n"
         "\n"
         " Options:\n"
         "    -workdir=dir - Use this directory as then work directory for\n"
         "     building the alignments instead of the work/align\n"
         "    -sortTmp=dir - Tmp dir for sort.\n"
         "    -orgCats=native,xeno - processon the specified organism \n"
         "     categories\n"
         "    -noMigrate - don't migrate existing alignments\n"
         "    -verbose=n - enable verbose output, values greater than 1\n"
         "     increase verbosity.\n"
         "\n"
         " Arguments:\n"
         "     relName - genbank or refseq release name (genbank.131.0).\n"
         "     update - full or daily.xxx\n"
         "     typeAccPrefix - type and access prefix, e.g. mrna, or\n"
         "       est.aj\n"
         "     db - database for the alignment\n"
         "\n");
}

int main(int argc, char* argv[])
{
char *relName, *updateName, *typeAccPrefix, *database, *sep;
struct gbIndex* index;
struct gbSelect select;
struct gbSelect* prevSelect = NULL;
boolean noMigrate;
ZeroVar(&select);

optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
gWorkDir = optionVal("workdir", "work/align");
gSortTmp = optionVal("sortTmp", NULL);
noMigrate = optionExists("noMigrate");
gbVerbInit(optionInt("verbose", 0));
relName = argv[1];
updateName = argv[2];
typeAccPrefix = argv[3];
database = argv[4];

/* parse typeAccPrefix */
sep = strchr(typeAccPrefix, '.');
if (sep != NULL)
    *sep = '\0';
select.type = gbParseType(typeAccPrefix);
if (sep != NULL)
    {
    select.accPrefix = sep+1;
    *sep = '.';
    }

index = gbIndexNew(database, NULL);
select.release = gbIndexMustFindRelease(index, relName);
select.update = gbReleaseMustFindUpdate(select.release, updateName);
select.orgCats = gbParseOrgCat(optionVal("orgCats", "native,xeno"));

gbVerbMsg(0, "gbAlignInstall: %s/%s/%s/%s", select.release->name,
          select.release->genome->database, select.update->name,
          typeAccPrefix);

/* Get the release to migrate, if applicable */
if (!noMigrate)
    prevSelect = gbAlignGetMigrateRel(&select);

gbAlignInstall(&select, prevSelect);

/* must go to stderr to be logged */
gbVerbMsg(0, "gbAlignInstall: complete");
    
gbIndexFree(&index);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
