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

static char const rcsid[] = "$Id: gbAlignInstall.c,v 1.3 2003/06/20 04:37:03 markd Exp $";

/*
 * Notes:
 *  - sorting into chromosome order is done here so that migrated alignments
 *    will also be sorted.
 */

/* FIXME: a wrapper around FILE that counts records written would simplify
 * some of this */

/* PSL sort spec: tName tStart tEnd qName qStart qEnd
 */
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
    {"verbose", OPTION_INT},
    {NULL, 0}
};


/* global parameters from command line */
static char* gWorkDir;
static char* gSortTmp;

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

struct migrateAligns
/* Object to collect info about alignments to migrated.
 * This is a little tricky, in that alignment records are migrated even
 * if the accession didn't align. */
{
    struct gbSelect* select;        /* selection criteria */
    struct gbSelect* prevSelect;    /* previous release info */
    int accCnt;                     /* number acc to migrated */
    int alignCnt;                   /* number PSLs to migrated */
    int pslMigrated;                /* number migrated */
    int oiMigrated;
    int intronPslMigrated;
};

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
for (nSort = 2; sortSpec[nSort] != NULL; nSort++)
    continue;
if (gSortTmp != NULL)
    nSort += 2;
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
 * part of the current update.  This handles this an ingored accession being
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

boolean countAlign(struct gbSelect* select, char* acc, unsigned version,
                   char* refFile)
/* Increment the count of alignment records for a object; return false
 * if entry should be skipped and not counted. */
{
struct gbEntry* entry = getEntry(select, acc, refFile);
if (entry == NULL)
    return FALSE;
else
    {
    struct gbAligned* aligned = gbEntryGetAligned(entry, select->update,
                                                  version);
    gbAlignedCount(aligned, 1);
    return TRUE;
    }
}

void migratePsl(struct migrateAligns* migrate, struct psl* psl, char* inPsl,
                FILE* outPslFh)
/* Migrate PSL, if it's accession and version are flagged */
{
char acc[GB_ACC_BUFSZ];
unsigned version;
struct gbEntry* entry;

version = gbSplitAccVer(psl->qName, acc);
entry = getEntry(migrate->prevSelect, acc, inPsl);

if ((entry != NULL) && (version == entry->selectVer))
    {
    if (countAlign(migrate->select, acc, version, inPsl))
        {
        if (verbose >= 3)
            gbVerbPr(3, "migrate psl %s.%d", acc, version);
        pslTabOut(psl, outPslFh);
        migrate->pslMigrated++;
        }
    }
}

void migratePsls(struct migrateAligns* migrate, char* outPsl, FILE* outPslFh)
/* Migrate selected PSL records */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;

gbAlignedGetPath(migrate->prevSelect, "psl.gz", NULL, inPsl);

gbVerbEnter(2, "migrating from %s", inPsl);
inPslLf = gzLineFileOpen(inPsl);
while ((psl = pslNext(inPslLf)) != NULL)
    {
    migratePsl(migrate, psl, inPsl, outPslFh);
    pslFree(&psl);
    }
gzLineFileClose(&inPslLf);

if (migrate->pslMigrated != migrate->alignCnt)
    errAbort("expected to migrate %d PSLs from %s to %s, found %d",
             migrate->alignCnt, inPsl, outPsl, migrate->pslMigrated);
gbVerbLeave(2, "migrating from %s", inPsl);
}

void migrateOrientInfo(struct migrateAligns* migrate,
                       struct estOrientInfo* oi, char* inOi, FILE* outOiFh)
/* Migrate a estOrientInfo row, if it's accession and version are flagged */
{
char acc[GB_ACC_BUFSZ];
unsigned version;
struct gbEntry* entry;

version = gbSplitAccVer(oi->name, acc);
entry = getEntry(migrate->prevSelect, acc, inOi);
if ((entry != NULL) && (version == entry->selectVer))
    {
    if (getEntry(migrate->select, acc, inOi) != NULL)
        {
        if (verbose >= 3)
            gbVerbPr(3, "migrate oi %s.%d", acc, version);
        estOrientInfoTabOut(oi, outOiFh);
        migrate->oiMigrated++;
        }
    }
}

void migrateOrientInfos(struct migrateAligns* migrate, char* outOi, 
                        FILE* outOiFh)
/* Migrate estOrientInfo records */
{
char inOi[PATH_LEN];
struct lineFile* inOiLf;
char *row[EST_ORIENT_INFO_NUM_COLS];

gbAlignedGetPath(migrate->prevSelect, "oi.gz", NULL, inOi);

gbVerbEnter(2, "migrating from %s", inOi);
inOiLf = gzLineFileOpen(inOi);
while (lineFileNextRowTab(inOiLf, row, ArraySize(row)))
    {
    struct estOrientInfo *oi = estOrientInfoLoad(row);
    migrateOrientInfo(migrate, oi, inOi, outOiFh);
    estOrientInfoFree(&oi);
    }
gzLineFileClose(&inOiLf);

/* Nasty issue: orientInfo records do not have a query range, yet there
 * are a few cases where different parts of the same mRNA aligned to
 * the same ramge of the genome.  This means we can't verify the number of
 * OI rows. */
#if 0
// FIXME: OI can be duplicated 
if (migrate->oiMigrated != migrate->alignCnt)
    errAbort("expected to migrate %d orientInfo records from %s to %s, found %d",
             migrate->alignCnt, inOi, outOi, migrate->oiMigrated);
#endif

gbVerbLeave(2, "migrating from %s", inOi);
}

void migrateIntronPsl(struct migrateAligns* migrate, struct psl* psl,
                      char* inPsl, FILE* outPsl)
/* Migrate an intron PSL, if it's accession and version are flagged */
{
char acc[GB_ACC_BUFSZ];
unsigned version;
struct gbEntry* entry;

version = gbSplitAccVer(psl->qName, acc);
entry = getEntry(migrate->prevSelect, acc, inPsl);
if ((entry != NULL) && (version == entry->selectVer))
    {
    if (getEntry(migrate->select, acc, inPsl) != NULL)
        {
        if (verbose >= 3)
            gbVerbPr(3, "migrate intronPsl %s.%d", acc, version);
        pslTabOut(psl, outPsl);
        migrate->intronPslMigrated++;
        }
    }
}

void migrateIntronPsls(struct migrateAligns* migrate, FILE* outPsl)
/* Migrate selected intronPsl records */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;

/* intron PSL files are not required to exist */
gbAlignedGetPath(migrate->prevSelect, "intronPsl.gz", NULL, inPsl);
if (fileExists(inPsl))
    {
    gbVerbEnter(2, "migrating from %s", inPsl);
    inPslLf = gzLineFileOpen(inPsl);
    while ((psl = pslNext(inPslLf)) != NULL)
        {
        migrateIntronPsl(migrate, psl, inPsl, outPsl);
        pslFree(&psl);
        }
    gzLineFileClose(&inPslLf);
    gbVerbLeave(2, "migrating from %s", inPsl);
    }
}

void migrateCallback(struct gbSelect* select, struct gbProcessed* processed,
                     struct gbAligned* prevAligned, void* clientData)
/* Function check if an alignment should be migrated. */
{
/* grab this alignment if it is in the update being processed, and the type or
 * orgCat havn't changed */
struct migrateAligns* migrateAligns = (struct migrateAligns*)clientData;

if ((prevAligned != NULL)
    && (prevAligned->update == migrateAligns->prevSelect->update)
    && (prevAligned->entry->type == processed->entry->type)
    && (prevAligned->entry->orgCat == processed->entry->orgCat))
    {
    if (verbose >= 3)
        gbVerbPr(3, "will migrate %d alignments for %s.%d",
                 prevAligned->numAligns, processed->entry->acc,
                 processed->version);
    prevAligned->entry->selectVer = processed->version;
    migrateAligns->accCnt++;
    migrateAligns->alignCnt += prevAligned->numAligns;
    }
}

unsigned migrateAlignedUpdate(struct gbSelect* select,
                              struct gbSelect* prevSelect,
                              char* outPsl, FILE* outPslFh,
                              char* outOi, FILE* outOiFh,
                              char* outIntronPsl, FILE* outIntronPslFh,
                              unsigned* intronPslCnt)
/* Migrate existing aligned PSLs from an earlier release update. */
{
struct migrateAligns migrate;
ZeroVar(&migrate);
migrate.select = select;
migrate.prevSelect = prevSelect;

/* build list of sequence to align */
gbAlignFindNeedAligned(select, prevSelect, migrateCallback, &migrate);
    
/* Copy records if any were found */
if (migrate.alignCnt > 0)
    {
    migratePsls(&migrate, outPsl, outPslFh);
    if(outOiFh != NULL)
        migrateOrientInfos(&migrate, outOi, outOiFh);
    if (outIntronPslFh != NULL)
        migrateIntronPsls(&migrate, outIntronPslFh);
    }
gbUpdateClearSelectVer(prevSelect->update);
*intronPslCnt += migrate.intronPslMigrated;
return migrate.pslMigrated;
}

unsigned migrateAligned(struct gbSelect* select, struct gbSelect* prevSelect,
                        char* outPsl, FILE* outPslFh,
                        char* outOi, FILE* outOiFh,
                        char* outIntronPsl, FILE* outIntronPslFh,
                        unsigned* intronPslCnt)
/* Migrate existing aligned PSLs from an earlier release. */
{
struct gbUpdate* prevUpdateHold = prevSelect->update;
struct gbUpdate* prevUpdate = prevSelect->release->updates;
unsigned pslCnt = 0;

/* traverse all updates in the previous release */
gbVerbEnter(1, "migrating alignments");
while (prevUpdate != NULL)
    {
    prevSelect->update = prevUpdate;
    pslCnt += migrateAlignedUpdate(select, prevSelect, outPsl, outPslFh,
                                   outOi, outOiFh,
                                   outIntronPsl, outIntronPslFh, intronPslCnt);
    prevUpdate = prevUpdate->next;
    }
prevSelect->update = prevUpdateHold;
gbVerbLeave(1, "migrating alignments");
return pslCnt;
}

void copyPsl(struct gbSelect* select, struct psl* psl, char* inPsl,
             FILE* outPslFh, int* count)
/* Copy a PSL. */
{
char acc[GB_ACC_BUFSZ];
int version = gbSplitAccVer(psl->qName, acc);
if (countAlign(select, acc, version, inPsl))
    {
    if (verbose >= 3)
        gbVerbPr(3, "install psl %s.%d", acc, version);
    pslTabOut(psl, outPslFh);
    (*count)++;
    }
}

unsigned copyPsls(struct gbSelect* select, FILE* outPslFh)
/* Copy a PSL file from the work directory if it exists, count alignments
 * for index. */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;
unsigned count = 0;

gbAlignedGetPath(select, "psl", gWorkDir, inPsl);
if (fileExists(inPsl))
    {
    gbVerbEnter(2, "installing from %s", inPsl);
    inPslLf = gzLineFileOpen(inPsl);
    while ((psl = pslNext(inPslLf)) != NULL)
        {
        copyPsl(select, psl, inPsl, outPslFh, &count);
        pslFree(&psl);
        }
    gzLineFileClose(&inPslLf);
    gbVerbLeave(2, "installing from %s", inPsl);
    }
return count;
}

void copyOrientInfo(struct gbSelect* select, struct estOrientInfo* oi,
                    char* inOi, FILE* outOiFh, int* count)
/* Copy a orientInfo record. */
{
char acc[GB_ACC_BUFSZ];
gbSplitAccVer(oi->name, acc);
if (getEntry(select, acc, inOi) != NULL)
    {
    if (verbose >= 3)
        gbVerbPr(3, "install oi %s", oi->name);
    estOrientInfoTabOut(oi, outOiFh);
    (*count)++;
    }
}

unsigned copyOrientInfos(struct gbSelect* select, FILE* outOiFh)
/* Copy an OI file from the work directory, if it exists, count alignments
 * for index. */
{
char inOi[PATH_LEN];
struct lineFile* inOiLf;
char *row[EST_ORIENT_INFO_NUM_COLS];
unsigned count = 0;

gbAlignedGetPath(select, "oi", gWorkDir, inOi);
if (fileExists(inOi))
    {
    gbVerbEnter(2, "installing from %s", inOi);
    inOiLf = gzLineFileOpen(inOi);
    while (lineFileNextRowTab(inOiLf, row, ArraySize(row)))
        {
        struct estOrientInfo *oi = estOrientInfoLoad(row);
        copyOrientInfo(select, oi, inOi, outOiFh, &count);
        estOrientInfoFree(&oi);
        }
    gzLineFileClose(&inOiLf);
    gbVerbLeave(2, "installing from %s", inOi);
    }
return count;
}

void copyIntronPsl(struct gbSelect* select, struct psl* psl, char* inPsl,
                   FILE* outPslFh, int* count)
/* Copy an intronPsl. */
{
char acc[GB_ACC_BUFSZ];
gbSplitAccVer(psl->qName, acc);
if (getEntry(select, acc, inPsl) != NULL)
    {
    if (verbose >= 3)
        gbVerbPr(3, "install intronPsl %s", psl->qName);
    pslTabOut(psl, outPslFh);
    (*count)++;
    }
}

unsigned copyIntronPsls(struct gbSelect* select, FILE* outPslFh)
/* Copy an intron PSL file from the work directory if it exists */
{
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;
unsigned count = 0;

gbAlignedGetPath(select, "intronPsl", gWorkDir, inPsl);
if (fileExists(inPsl))
    {
    gbVerbEnter(2, "installing from %s", inPsl);
    inPslLf = gzLineFileOpen(inPsl);
    while ((psl = pslNext(inPslLf)) != NULL)
        {
        copyIntronPsl(select, psl, inPsl, outPslFh, &count);
        pslFree(&psl);
        }
    gzLineFileClose(&inPslLf);
    gbVerbLeave(2, "installing from %s", inPsl);
    }
return count;
}

void createIndexCallback(struct gbSelect* select, struct gbProcessed* processed,
                         struct gbAligned* prevAligned, void* clientData)
/* Callback to write index records from info save in index. If the
 * sequence doesn't have an aligned entry, it didn't aligned and is
 * recorded as such. */
{
FILE* alignIdxFh = (FILE*)clientData;
struct gbEntry* entry = processed->entry;
struct gbAligned* aligned = gbEntryFindAlignedVer(entry, processed->version);
int numAligns = ((aligned != NULL) ? aligned->numAligns : 0);

gbAlignedWriteIdxRec(alignIdxFh, entry->acc, processed->version, numAligns);
}

void createAlignedIndex(struct gbSelect* select, char* alignIdx)
/* create an alignment index from the alignRecs stored in the index.
 * it is not renamed from the tmp name here.
 */
{
FILE *alignIdxFh;

/* setup output PSL files */
gbAlignedGetPath(select, "alidx", NULL, alignIdx);
alignIdxFh = gbMustOpenOutput(alignIdx);

/* tarverse all that should be aligned */
gbAlignFindNeedAligned(select, NULL, createIndexCallback, alignIdxFh);

carefulClose(&alignIdxFh);
}

void installOrgCatAligned(struct gbSelect* select, unsigned orgCat,
                          struct gbSelect* prevSelect, char* alignIdx)
/* Install alignments for either native or xeno.  The alignment index is
 * created and named returned, but not renamed until both native and xeno are
 * processed. */
{
unsigned holdOrgCats = select->orgCats;
char outPsl[PATH_LEN], outOi[PATH_LEN], outIntronPsl[PATH_LEN];
FILE *outPslFh = NULL, *outOiFh = NULL, *outIntronPslFh = NULL;
unsigned alignCnt = 0, intronPslCnt = 0;

select->orgCats = orgCat;
if (prevSelect != NULL)
    prevSelect->orgCats = orgCat;

/* setup output PSL and orientInfo files */
gbAlignedGetPath(select, "psl.gz", NULL, outPsl);
outPslFh = openSortOutput(outPsl, PSL_SORT_SPEC);
if (select->orgCats == GB_NATIVE)
    {
    gbAlignedGetPath(select, "oi.gz", NULL, outOi);
    outOiFh = openSortOutput(outOi, OI_SORT_SPEC);
    if (select->type == GB_EST)
        {
        gbAlignedGetPath(select, "intronPsl.gz", NULL, outIntronPsl);
        outIntronPslFh = openSortOutput(outIntronPsl, PSL_SORT_SPEC);
        }
    }

/* previous aligned if this is a full update */
if (prevSelect != NULL)
    alignCnt += migrateAligned(select, prevSelect, outPsl, outPslFh, 
                               outOi, outOiFh, outIntronPsl, outIntronPslFh,
                               &intronPslCnt);

/* copy currently aligned, if they exist */
alignCnt += copyPsls(select, outPslFh);

if (select->orgCats == GB_NATIVE)
    {
    if (alignCnt > 0)
        {
        copyOrientInfos(select, outOiFh);
        gbOutputRename(outOi, &outOiFh);
        }
    else
        gbOutputRemove(outOi, &outOiFh);

    if (select->type == GB_EST)
        {
        intronPslCnt += copyIntronPsls(select, outIntronPslFh);
        if (intronPslCnt > 0)
            gbOutputRename(outIntronPsl, &outIntronPslFh);
        else
            gbOutputRemove(outIntronPsl, &outIntronPslFh);
        }
    }

if (alignCnt > 0)
    gbOutputRename(outPsl, &outPslFh);
else
    gbOutputRemove(outPsl, &outPslFh);

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

gbVerbEnter(1, "gbAlignInstall: %s", gbSelectDesc(select));

/* load required entry date */
gbReleaseLoadProcessed(select);
if (prevSelect != NULL)
    {
    gbReleaseLoadProcessed(prevSelect);
    gbReleaseLoadAligned(prevSelect);
    }

/* Process each category */
if (select->orgCats & GB_NATIVE)
    installOrgCatAligned(select, GB_NATIVE, prevSelect, nativeAlignIdx);
if (select->orgCats & GB_XENO)
    installOrgCatAligned(select, GB_XENO, prevSelect, xenoAlignIdx);

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

int main(int argc, char* argv[])
{
char *relName, *updateName, *typeAccPrefix, *database, *sep;
struct gbIndex* index;
struct gbSelect select;
struct gbSelect* prevSelect = NULL;
ZeroVar(&select);

optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
gWorkDir = optionVal("workdir", "work/align");
gSortTmp = optionVal("sortTmp", NULL);
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
select.orgCats = GB_NATIVE|GB_XENO;

gbVerbMsg(0, "gbAlignInstall: %s/%s/%s/%s", select.release->name,
          select.release->genome->database, select.update->name,
          typeAccPrefix);

/* Get the release to migrate, if applicable */
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
