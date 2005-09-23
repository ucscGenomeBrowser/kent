/* Get mRNA/EST alignment data */
#include "common.h"
#include "pslData.h"
#include "gbIndex.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "psl.h"

/* open output file */
static FILE* gOutPsl = NULL;

/* global options from command line */
static boolean gInclVersion;
static char* gGetWhat;

static void flagUnaligned(struct gbSelect* select, unsigned orgCat)
/* mark unaligned entries so that they are not reported as missing */
{
struct gbAligned* aln;
for (aln = select->update->aligned; aln != NULL; aln = aln->updateLink)
    {
    if ((aln->entry->orgCat & orgCat) && (aln->numAligns == 0))
        aln->entry->clientFlags = TRUE;
    }
}

static void processPsl(struct gbSelect* select, struct psl* psl)
/* process the next PSL from an update PSL file, possibly outputing
 * the alignment */
{
char acc[GB_ACC_BUFSZ];
short version = gbSplitAccVer(psl->qName, acc);

/* will return NULL on ignored sequences */
struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
if ((entry != NULL) && (version == entry->selectVer))
    {
    /* selected */
    if (!gInclVersion)
        strcpy(psl->qName, acc);  /* remove version */
    pslTabOut(psl, gOutPsl);
    entry->clientFlags = TRUE; /* flag so we know we got it */
    }
/* trace if enabled */
if (gbVerbose >= 3)
    {
    if (entry == NULL)
        gbVerbPr(3, "no entry: %s.%d", acc, version);
    else if (entry->selectVer <= 0)
        gbVerbPr(3, "not selected: %s.%d", acc, version);
    else if (version != entry->selectVer)
        gbVerbPr(3, "not version: %s.%d != %d", acc, version, entry->selectVer);
    else
        gbVerbPr(3, "save: %s.%d", acc, version);
    }
}

static void processOrgCatPsls(struct gbSelect* select, unsigned orgCat)
/* process files in an update an organism category */
{
char baseName[256];
char inPsl[PATH_LEN];
struct lineFile* inPslLf;
struct psl* psl;
unsigned orgCatsHold = select->orgCats;
select->orgCats = orgCat;

safef(baseName, sizeof(baseName), "%s.gz", gGetWhat);
gbAlignedGetPath(select, baseName, NULL, inPsl);

inPslLf = gzLineFileOpen(inPsl); 
while ((psl = pslNext(inPslLf)) != NULL)
    {
    processPsl(select, psl);
    pslFree(&psl);
    }
gzLineFileClose(&inPslLf);
select->orgCats = orgCatsHold;
}

void pslDataOpen(char* getWhat, boolean inclVersion, char *outFile)
/* open output file and set options. getWhat is psl or intronPsl */
{
gGetWhat = getWhat;
gInclVersion = inclVersion;
gOutPsl = gzMustOpen(outFile, "w");
}

void pslDataProcessUpdate(struct gbSelect* select)
/* Get PSL alignments for a partition and update.  Partition processed and
 * aligned indexes should be loaded and selected versions flaged. */
{
if (select->orgCats & GB_NATIVE)
    {
    if (select->update->numNativeAligns[gbTypeIdx(select->type)] > 0)
        processOrgCatPsls(select, GB_NATIVE);
    flagUnaligned(select, GB_NATIVE);
    }
if (select->orgCats & GB_XENO)
    {
    if (select->update->numXenoAligns[gbTypeIdx(select->type)] > 0)
        processOrgCatPsls(select, GB_XENO);
    flagUnaligned(select, GB_XENO);
    }
}

void pslDataClose()
/* close the output file */
{
gzClose(&gOutPsl);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
