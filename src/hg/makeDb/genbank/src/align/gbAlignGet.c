#include "gbAlignCommon.h"
#include "gbIndex.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbAligned.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "common.h"
#include "sqlNum.h"
#include "portable.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "gbFa.h"
#include <stdio.h>

static char const rcsid[] = "$Id: gbAlignGet.c,v 1.10 2005/09/14 21:10:04 markd Exp $";

/* version to set in gbEntry.selectVer to indicate an entry is being
 * migrated */
#define MIGRATE_VERSION -2  

/* BLAT crashed on a bogus EST that only had one base in the entry,
 * so we skip sequences under this size */
#define MIN_SEQ_SIZE 6

/* command line option specifications */
static struct optionSpec optionSpecs[] =
{
    {"fasize", OPTION_INT},
    {"workdir", OPTION_STRING},
    {"orgCats", OPTION_STRING},
    {"noMigrate", OPTION_BOOLEAN},
    {"polyASizes", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

/* global parameters from command line */
static char* workDir;
static unsigned maxFaSize;
static boolean createPolyASizes = FALSE;

struct outFa
/* Output fasta file info, opened in a lazy manner, split into native and
 * xeno, and partitioned to have no more than a specified max size */
{
    struct gbSelect select;   /* select info to open file */
    unsigned nextPartNum;     /* next partition number */
    struct gbFa* fa;          /* fasta object, NULL until opened */
    int numSeqs;              /* number of sequences written to current fasta */
    long long numBases;       /* number of base written */
    FILE *polyAFh;            /* file for poly-A sizes, if not NULL */
};

struct outFa* outFaNew(struct gbSelect* select, unsigned orgCat)
/* create a new, unopened output object */
{
struct outFa* outFa;
AllocVar(outFa);
outFa->select = *select;
assert((orgCat == GB_NATIVE) || (orgCat == GB_XENO));
outFa->select.orgCats = orgCat;
return outFa;
}

void outFaOpen(struct outFa* outFa)
/* Open the fasta file  */
{
char ext[64];
char path[PATH_LEN];
assert(outFa->fa == NULL);

safef(ext, sizeof(ext), "%d.fa", outFa->nextPartNum);
gbAlignedGetPath(&outFa->select, ext, workDir, path);
outFa->fa = gbFaOpen(path, "w");
outFa->numSeqs = 0;
outFa->numBases = 0;

if (createPolyASizes)
    {
    safef(ext, sizeof(ext), "%d.polya", outFa->nextPartNum);
    gbAlignedGetPath(&outFa->select, ext, workDir, path);
    outFa->polyAFh = mustOpen(path, "w");
    }
outFa->nextPartNum++;
}

void outFaClose(struct outFa* outFa)
/* close file and output file path.  Doesn't delete object */
{
if (outFa->fa != NULL)
    {
    printf("alignFa: %s %s %d %lld\n", outFa->fa->fileName,
           gbOrgCatName(outFa->select.orgCats), outFa->numSeqs,
           outFa->numBases);
    gbFaClose(&outFa->fa);
    carefulClose(&outFa->polyAFh);
    }
}

void outFaFree(struct outFa** outFaPtr)
/* Free object, closing if open */
{
if (*outFaPtr != NULL)
    outFaClose(*outFaPtr);
freez(outFaPtr);
}

void outFaWrite(struct outFa* outFa, struct gbFa* inFa)
/* write a record to the output fasta, open or switch to a new file
 * if needed. */
{
if ((maxFaSize > 0) && (outFa->fa != NULL) && (outFa->fa->off > maxFaSize))
    outFaClose(outFa);
if (outFa->fa == NULL)
    outFaOpen(outFa);
gbFaWriteFromFa(outFa->fa, inFa, NULL);
outFa->numSeqs++;
outFa->numBases += inFa->seqLen;

if (outFa->polyAFh != NULL)
    {
    /* note, this modifies the fasta sequence, but we don't care any more */
    fprintf(outFa->polyAFh, "%s\t%d\t%d\t%d\n", inFa->id, inFa->seqLen,
            maskTailPolyA(inFa->seq, inFa->seqLen),
            maskHeadPolyT(inFa->seq, inFa->seqLen));
    }
}

boolean copyFastaRec(struct gbSelect* select, struct gbFa* inFa,
                     struct outFa* nativeFa, struct outFa* xenoFa)
/* Read and copy a record to one of the output files, if selected */
{
char acc[GB_ACC_BUFSZ];
unsigned version;
struct gbEntry* entry;

if (!gbFaReadNext(inFa))
    return FALSE; /* EOF */

version = gbSplitAccVer(inFa->id, acc);
entry = gbReleaseFindEntry(select->release, acc);
if (entry != NULL)
    {
    char* seq = gbFaGetSeq(inFa);
    if (strlen(seq) < MIN_SEQ_SIZE)
        {
        if (gbVerbose >= 3)
            gbVerbPr(3, "skip %s, less than minimum sequence size", inFa->id);
        }
    else if ((version == entry->selectVer) && (entry->clientFlags & ALIGN_FLAG))
        {
        outFaWrite(((entry->orgCat == GB_NATIVE) ? nativeFa : xenoFa),  inFa);
        if (gbVerbose >= 3)
            gbVerbPr(3, "aligning %s %s", inFa->id,
                     gbOrgCatName(entry->orgCat));
        }
    else if ((version == entry->selectVer) && (entry->clientFlags & MIGRATE_FLAG))
        {
        if (gbVerbose >= 3)
            gbVerbPr(3, "migrating %s %s", inFa->id,
                     gbOrgCatName(entry->orgCat));
        }
    else 
        {
        assert(version != entry->selectVer);
        if (gbVerbose >= 3)
            gbVerbPr(3, "skip %s, wrong version %s != %d", 
                     gbOrgCatName(entry->orgCat), inFa->id,
                     entry->selectVer);
        }
    }
else
    {
    if (gbVerbose >= 3)
        gbVerbPr(3, "skip %s, no entry", inFa->id);
    }

return TRUE;
}

void copySelectedFasta(struct gbSelect* select)
/* copy FASTA records that were selected for alignment, segregating by
 * native/xeno, and partitioning large files. */
{
char inFasta[PATH_LEN];
struct gbFa* inFa;
struct outFa* nativeFa = NULL;
struct outFa* xenoFa = NULL;
if (select->orgCats & GB_NATIVE)
    nativeFa = outFaNew(select, GB_NATIVE);
if (select->orgCats & GB_XENO)
    xenoFa = outFaNew(select, GB_XENO);

gbProcessedGetPath(select, "fa", inFasta);
gbVerbEnter(2, "copying from %s", inFasta);
inFa = gbFaOpen(inFasta, "r");

while (copyFastaRec(select, inFa, nativeFa, xenoFa))
    continue;

outFaFree(&nativeFa);
outFaFree(&xenoFa);
gbFaClose(&inFa);
gbVerbLeave(2, "copying from %s", inFasta);
}

void markAligns(struct gbSelect* select, unsigned orgCat, struct gbAlignInfo *alignInfo)
/* create a file indicating that sequences either needs aligned or migated for
 * this for this partation.  This is used to determine what needs to be
 * installed after the alignment.  This is needed  because they might be all
 * be migrate, so that fasta can't be the indicator.  If there is nothing
 * to process, just create an empty alidx so that no more processing is
 * done. This is a performance win to prevent gbAlignInstall from parsing
 * indixes again. */
{
char path[PATH_LEN];
FILE* fh;
unsigned orgCatsHold = select->orgCats;
select->orgCats = orgCat;

if (gbEntryCntsHaveAny(&alignInfo->migrate, orgCat) || gbEntryCntsHaveAny(&alignInfo->align, orgCat))
    gbAlignedGetPath(select, "aligns", workDir, path);
else
    gbAlignedGetIndex(select, path);

fh = gbMustOpenOutput(path);
gbOutputRename(path, &fh);

select->orgCats = orgCatsHold;
}

struct gbAlignInfo gbAlignGet(struct gbSelect* select,
                              struct gbSelect* prevSelect)
/* Build files to align in the work directory.  If this is not a full release,
 * or there is no previously aligned release, prevSelect should be NULL.
 */
{
gbVerbEnter(1, "gbAlignGet: %s", gbSelectDesc(select));
if (prevSelect != NULL)
    prevSelect->orgCats = select->orgCats;

/* load the required entry data */
gbReleaseLoadProcessed(select);
if (prevSelect != NULL)
    {
    gbReleaseLoadProcessed(prevSelect);
    gbReleaseLoadAligned(prevSelect);
    }

/* select entries to align */
gbVerbEnter(2, "selecting seqs to align");
struct gbAlignInfo alignInfo = gbAlignFindNeedAligned(select, prevSelect);
gbVerbLeave(2, "selecting seqs to align");

if (alignInfo.migrate.accTotalCnt > 0)
    gbVerbMsg(1, "gbAlignGet: %d %s entries, %d alignments will be migrated",
              alignInfo.migrate.accTotalCnt, gbFmtSelect(select->type),
              alignInfo.migrate.recTotalCnt);

/* create fasta with sequences to align if not empty */
if (alignInfo.align.accTotalCnt > 0)
    {
    gbVerbMsg(1, "gbAlignGet: %d %s sequences will be align",
              alignInfo.align.accTotalCnt, gbFmtSelect(select->type));
    copySelectedFasta(select);
    }

/* leave calling cards */
if (select->orgCats & GB_NATIVE)
    markAligns(select, GB_NATIVE, &alignInfo);
if (select->orgCats & GB_XENO)
    markAligns(select, GB_XENO, &alignInfo);

/* print before releasing memory */
gbVerbLeave(1, "gbAlignGet: %s", gbSelectDesc(select));

/* unload entries to free memory */
gbReleaseUnload(select->release);
if (prevSelect != NULL)
    gbReleaseUnload(prevSelect->release);
return alignInfo;
}

void usage()
/* print usage and exit */
{
errAbort("   gbAlignGet [options] relname update typeAccPrefix db\n"
         "\n"
         "Get sequences for GenBank alignment step. This creates FASTA\n"
         "files in the aligned work directory for an update to\n"
         "align with the genome.  If the full update is being\n"
         "processed and there is a previous release, the existing PSL\n"
         "alignments will be migrate and are not selected for alignment.\n"
         "Note that the full update should be the first processed in a\n"
         "release for migration to work.\n"
         "\n"
         " Options:\n"
         "    -workdir=dir Use this directory as then work directory for\n"
         "     building the alignments instead of the work/align/\n"
         "    -fasize=n - Set the maximum fasta file size to be about n\n"
         "     bytes. Fasta files are split for cluster partitioning. \n"
         "    -orgCats=native,xeno - processon the specified organism \n"
         "     categories\n"
         "    -polyASizes - If specified, create *.polya files for each fasta file,\n"
         "     in the same format as faPolyASizes.\n"
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
struct gbAlignInfo alignInfo;
boolean noMigrate;
ZeroVar(&select);

optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
maxFaSize = optionInt("fasize", -1);
workDir = optionVal("workdir", "work/align");
noMigrate = optionExists("noMigrate");
createPolyASizes = optionExists("polyASizes");
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
select.orgCats = gbParseOrgCat(optionVal("orgCats", "native,xeno"));

index = gbIndexNew(database, NULL);
select.release = gbIndexMustFindRelease(index, relName);
select.update = gbReleaseMustFindUpdate(select.release, updateName);
gbVerbMsg(0, "gbAlignGet: %s/%s/%s/%s", select.release->name,
          select.release->genome->database, select.update->name,
          typeAccPrefix);

/* Get the release to migrate, if applicable */
if (!noMigrate)
    prevSelect = gbAlignGetMigrateRel(&select);

alignInfo = gbAlignGet(&select, prevSelect);

/* always print stats */
fprintf(stderr, "gbAlignGet: %s/%s/%s/%s: align=%d, migrate=%d\n",
        select.release->name, select.release->genome->database,
        select.update->name, typeAccPrefix,
        alignInfo.align.accTotalCnt, alignInfo.migrate.accTotalCnt);
gbIndexFree(&index);

/* print alignment and migrate count, which is read by the driver program */
printf("alignCnt: %d %d\n", alignInfo.align.accTotalCnt, alignInfo.migrate.accTotalCnt);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

