/* Get mRNA/EST sequence data */
#include "common.h"
#include "seqData.h"
#include "gbIndex.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "gbFa.h"

/* maximum faction of sequence that can be invalid mRNA characters */
#define MAX_INVALID_MRNA_BASES 0.01

/* open output file */
static struct gbFa* gOutFa = NULL;

/* global options from command line */
static boolean gInclVersion;

static boolean isValidMrnaSeq(struct gbFa* inFa)
/* check if the sequence appears to be a valid mrna sequence */
{
char* seq = gbFaGetSeq(inFa);
int numInvalid = numAllowedRNABases(seq);
int maxInvalid = MAX_INVALID_MRNA_BASES * inFa->seqLen;
if ((MAX_INVALID_MRNA_BASES > 0.0) && (maxInvalid == 0))
    maxInvalid = 1;  /* round up */
return (numInvalid <= maxInvalid);
}

static void processSeq(struct gbSelect* select, struct gbFa* inFa)
/* process the next sequence from an update fasta file, possibly outputing
 * the sequence */
{
char acc[GB_ACC_BUFSZ], hdrBuf[GB_ACC_BUFSZ], *hdr = NULL;
short version = gbSplitAccVer(inFa->id, acc);

/* will return NULL on ignored sequences */
struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);

if ((entry != NULL) && (version == entry->selectVer) && !entry->clientFlags)
    {
    /* selected, output if it appears valid */
    if (isValidMrnaSeq(inFa))
        {
        if (!gInclVersion)
            {
            /* put version in comment */
            safef(hdrBuf, sizeof(hdrBuf), "%s %d", acc, version);
            hdr = hdrBuf;
            }
        gbFaWriteFromFa(gOutFa, inFa, hdr);
        entry->clientFlags = TRUE; /* flag so only gotten once */
        }
    else
        {
        fprintf(stderr, "warning: %s does not appear to be a valid mRNA sequence, skipped: %s:%d\n",
                inFa->id, inFa->fileName, inFa->recLineNum);
        }
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

void seqDataOpen(boolean inclVersion, char *outFile)
/* open output file and set options */
{
gInclVersion = inclVersion;
gOutFa = gbFaOpen(outFile, "w");
}

void seqDataProcessUpdate(struct gbSelect* select)
/* Get sequences for a partition and update.  Partition processed index should
 * be loaded and selected versions flaged. */
{
char inFasta[PATH_LEN];
struct gbFa* inFa;
gbProcessedGetPath(select, "fa", inFasta);
inFa = gbFaOpen(inFasta, "r"); 
while (gbFaReadNext(inFa))
    processSeq(select, inFa);
gbFaClose(&inFa);
}

void seqDataClose()
/* close the output file */
{
gbFaClose(&gOutFa);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

