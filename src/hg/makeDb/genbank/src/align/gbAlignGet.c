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

static char const rcsid[] = "$Id: gbAlignGet.c,v 1.1 2003/06/03 01:27:42 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"fasize", OPTION_INT},
    {"workdir", OPTION_STRING},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

/* global parameters from command line */
static char* workDir;
static unsigned maxFaSize;

struct alignCnts
/* Counts of alignments */
{
    unsigned alignCnt;
    unsigned migrateCnt;
    unsigned nativeCnt;
    unsigned xenoCnt;
};

void alignCntsSum(struct alignCnts* accum, struct alignCnts* cnts)
/* add fields in cnts to accum */
{
accum->alignCnt += cnts->alignCnt;
accum->migrateCnt += cnts->migrateCnt;
accum->nativeCnt += cnts->nativeCnt;
accum->xenoCnt += cnts->xenoCnt;
}

struct outFa
/* Output fasta file info, opened in a lazy manner, split into native and
 * xeno, and partitioned to have no more than a specified max size */
{
    struct gbSelect select;   /* select info to open file */
    unsigned nextPartNum;     /* next partition number */
    struct gbFa* fa;          /* fasta object, NULL until opened */
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

safef(ext, sizeof(ext), "%d.fa.gz", outFa->nextPartNum++);
gbAlignedGetPath(&outFa->select, ext, workDir, path);
outFa->fa = gbFaOpen(path, "w");
}

void outFaClose(struct outFa* outFa)
/* close file and output file path.  Doesn't delete object */
{
if (outFa->fa != NULL)
    {
    printf("alignFa: %s\n", outFa->fa->fileName);
    gbFaClose(&outFa->fa);
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
}

boolean copyFastaRec(struct gbSelect* select, struct gbFa* inFa,
                     struct outFa* nativeFa, struct outFa* xenoFa)
/* Read and copy a record to one of the output files */
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
    if (version == entry->selectVer)
        {
        outFaWrite(((entry->orgCat == GB_NATIVE) ? nativeFa : xenoFa), inFa );
        if (verbose >= 3)
            gbVerbPr(3, "align: %s %s", inFa->id,
                     ((entry->orgCat == GB_NATIVE) ? "native": "xeno"));
        }
    else
        {
        if (verbose >= 3)
            gbVerbPr(3, "skip, version doesn't match: %s != %d", inFa->id,
                     entry->selectVer);
        }
    }
else
    if (verbose >= 3)
        gbVerbPr(3, "skip, no entry: %s.%d", inFa->id, version);


return TRUE;
}

void copySelectedFasta(struct gbSelect* select)
/* copy FASTA records that were selected for alignment, segregating by
 * native/xeno, and partitioning large files. */
{
char inFasta[PATH_LEN];
struct gbFa* inFa;
struct outFa* nativeFa = outFaNew(select, GB_NATIVE);
struct outFa* xenoFa = outFaNew(select, GB_XENO);

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

void needAlignedCallback(struct gbSelect* select,
                         struct gbProcessed* processed,
                         struct gbAligned* prevAligned,
                         void* clientData)
/* Function called for each sequence that needs aligned */
{
struct alignCnts* alignCnts = clientData;

/* Skip if sequence is aligned in the previous release, otherwise mark the
 * entry with the version.  Don't migrate if type or orgCat changed */
if ((prevAligned != NULL)
    && (prevAligned->entry->type == processed->entry->type)
    && (prevAligned->entry->orgCat == processed->entry->orgCat))
    {
    if (verbose >= 3)
        gbVerbPr(3, "migrate %s.%d", processed->entry->acc,
                 processed->version);
    alignCnts->migrateCnt++;
    }
else
    {
    if (verbose >= 3)
        gbVerbPr(3, "need to align %s.%d", processed->entry->acc,
                 processed->version);
    processed->entry->selectVer = processed->version;
    alignCnts->alignCnt++;
    }
if (processed->entry->orgCat == GB_NATIVE)
    alignCnts->nativeCnt++;
else
    alignCnts->xenoCnt++;
}

void markAligns(struct gbSelect* select, unsigned orgCat)
/* create a file indicating that sequences either needs aligned or migated for
 * this for this partation.  This is used to determine what needs to be
 * installed after the alignment.  This is needed  because they might be all
 * be migrate, so that fasta can't be the indicator. */
{
char path[PATH_LEN];
FILE* fh;
unsigned orgCatsHold = select->orgCats;
select->orgCats = orgCat;

gbAlignedGetPath(select, "aligns", workDir, path);
fh = gbMustOpenOutput(path);
gbOutputRename(path, &fh);

select->orgCats = orgCatsHold;
}

struct alignCnts alignGet(struct gbSelect* select, struct gbSelect* prevSelect,
                          unsigned orgCats)
/* Build files to align in the work directory.  If this is not a full release,
 * or there is no previously aligned release, prevGenome should be NULL.
 */
{
struct alignCnts alignCnts;
ZeroVar(&alignCnts);

gbVerbEnter(1, "gbAlignGet: %s", gbSelectDesc(select));
select->orgCats = orgCats;
if (prevSelect != NULL)
    prevSelect->orgCats = orgCats;

/* load required entry date */
gbReleaseLoadProcessed(select);
if (prevSelect != NULL)
    {
    gbReleaseLoadProcessed(prevSelect);
    gbReleaseLoadAligned(prevSelect);
    }

/* select entries to align */
gbVerbEnter(2, "selecting seqs to align");
gbAlignFindNeedAligned(select, prevSelect, needAlignedCallback, &alignCnts);
gbVerbLeave(2, "selecting seqs to align");
    
if (alignCnts.migrateCnt > 0)
    gbVerbMsg(1, "gbAlignGet: %d %s sequences will be migrated",
              alignCnts.migrateCnt, gbFmtSelect(select->type));

/* create fasta with sequences to align if not empty */
if (alignCnts.alignCnt > 0)
    {
    gbVerbMsg(1, "gbAlignGet: %d %s sequences will be align",
              alignCnts.alignCnt, gbFmtSelect(select->type));
    copySelectedFasta(select);
    }

/* leave calling cards */
markAligns(select, GB_NATIVE);
if (select->release->srcDb != GB_REFSEQ)
    markAligns(select, GB_XENO);

/* print before releasing memory */
gbVerbLeave(1, "gbAlignGet: %s", gbSelectDesc(select));

/* unload entries to free memory */
gbReleaseUnload(select->release);
if (prevSelect != NULL)
    gbReleaseUnload(prevSelect->release);
return alignCnts;
}

struct alignCnts gbAlignGet(struct gbSelect* select,
                            struct gbSelect* prevSelect)
/* Build files to align in the work directory.  If this is not a full release,
 * or there is no previously aligned release, prevGenome should be NULL. */
{
struct alignCnts cnts;
struct alignCnts alignCnts;
ZeroVar(&alignCnts);

if (select->release->srcDb == GB_GENBANK)
    {
    /* genbank, native and xeno */
    cnts = alignGet(select, prevSelect, GB_NATIVE|GB_XENO);
    alignCntsSum(&alignCnts, &cnts);
    }
else
    {
    /* refseq, just native */
    cnts = alignGet(select, prevSelect, GB_NATIVE);
    alignCntsSum(&alignCnts, &cnts);
    }
return alignCnts;
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
struct alignCnts alignCnts;
ZeroVar(&select);

optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
maxFaSize = optionInt("fasize", -1);
workDir = optionVal("workdir", "work/align");
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
select.orgCats = GB_NATIVE|GB_XENO;

index = gbIndexNew(database, NULL);
select.release = gbIndexMustFindRelease(index, relName);
select.update = gbReleaseMustFindUpdate(select.release, updateName);
gbVerbMsg(0, "gbAlignGet: %s/%s/%s/%s", select.release->name,
          select.release->genome->database, select.update->name,
          typeAccPrefix);

/* Get the release to migrate, if applicable */
prevSelect = gbAlignGetMigrateRel(&select);

alignCnts = gbAlignGet(&select, prevSelect);

/* always print stats */
fprintf(stderr, "gbAlignGet: %s/%s/%s/%s: align=%d, migrate=%d\n",
        select.release->name, select.release->genome->database,
        select.update->name, typeAccPrefix,
        alignCnts.alignCnt, alignCnts.migrateCnt);
gbIndexFree(&index);

/* print alignment count, which is read by the driver program */
printf("alignCnt: %d\n", alignCnts.alignCnt);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

