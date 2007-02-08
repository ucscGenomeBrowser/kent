/* txPsltoBed - Convert a psl to a bed file by projecting it onto it's target 
 * sequence. Optionally merge adjacent blocks and trim to splice sites. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "dnaseq.h"
#include "nibTwo.h"
#include "bed.h"

/* Variables set from command line. */
int mergeMax = 5;	
boolean fixIntrons = TRUE;
char *dnaPath = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txPsltoBed - Convert a psl to a bed file by projecting it onto its target\n"
  "sequence. Optionally merge adjacent blocks and trim to splice sites.\n"
  "usage:\n"
  "   txPsltoBed input.psl output.bed\n"
  "options:\n"
  "   -mergeMax=N - merge small blocks separated by no more than N on either\n"
  "                 target or query. Default value is %d.\n"
  "   -noFixIntrons - slide large gaps in target to seek for splice sites.\n"
  "   -dnaPath=path - path to DNA - either a two bit file or a dir of nib files.\n"
  , mergeMax
  );
}

static struct optionSpec options[] = {
   {"mergeMax", OPTION_INT},
   {"noFixIntrons", OPTION_BOOLEAN},
   {"dnaPath", OPTION_STRING},
   {NULL, 0},
};

struct bed *pslToBed(struct psl *psl, int mergeMax)
/* Create a bed similar to PSL, but merging nearby blocks. */
{
/* Calculate blockCount. */
int blockCount = 1;
int tStart = psl->tStarts[0];
int qStart = psl->qStarts[0];
int blockSize = psl->blockSizes[0];
int i;
for (i=1; i<psl->blockCount; ++i)
    {
    int tEnd = tStart + blockSize;
    int qEnd = qStart + blockSize;
    tStart = psl->tStarts[i];
    qStart = psl->qStarts[i];
    blockSize = psl->blockSizes[i];
    if (tStart - tEnd > mergeMax || qStart - qEnd > mergeMax)
	++blockCount;
    }

/* Call library routine that does conversion without merging. */
struct bed *bed = bedFromPsl(psl);
if (blockCount != psl->blockCount)
    {
    /* If need be merge some blocks. */
    blockCount = 1;
    int tStart = psl->tStarts[0];
    int qStart = psl->qStarts[0];
    int blockSize = psl->blockSizes[0];
    for (i=1; i<psl->blockCount; ++i)
        {
	int tEnd = tStart + blockSize;
	int qEnd = qStart + blockSize;
	tStart = psl->tStarts[i];
	qStart = psl->qStarts[i];
	blockSize = psl->blockSizes[i];
	if (tStart - tEnd > mergeMax || qStart - qEnd > mergeMax)
	    {
	    bed->blockSizes[blockCount] = blockSize;
	    bed->chromStarts[blockCount] = tStart - bed->chromStart;
	    ++blockCount;
	    }
	else
	    {
	    bed->blockSizes[blockCount-1] += tStart - tEnd + blockSize;
	    }
	}
    bed->blockCount = blockCount;
    }
return bed;
}

void fixBedIntrons(struct bed *bed, struct dnaSeq *chrom)
/* Go through gaps in bed.  For ones the right size to be introns,
 * see if ends are introns.  If not see if adding or subtracting
 * a base could turn them into intron ends. */
{
int start = bed->chromStarts[0];
int size = bed->blockSizes[0];
int i;
char *dna = chrom->dna;
int orientation = (bed->strand[0] == '-' ? -1 : 1);
for (i=1; i<bed->blockCount; ++i)
    {
    int end = start+size;
    size = bed->blockSizes[i];
    start = bed->chromStarts[i];
    char *intronStart = dna+end+bed->chromStart;
    char *intronEnd = dna+start+bed->chromStart;
    if (intronOrientation(intronStart, intronEnd) != orientation)
        {
	if (orientation > 0)
	    {
	    if (memcmp(intronStart, "gt", 2) != 0)
		{
		if (memcmp(intronStart-1, "gt", 2) == 0)
		    {
		    bed->blockSizes[i-1] -= 1;
		    }
		else if (memcmp(intronStart+1, "gt", 2) == 0)
		    {
		    bed->blockSizes[i-1] += 1;
		    }
		}
	    if (memcmp(intronEnd-2, "ag", 2) != 0)
	        {
		if (memcmp(intronEnd-2-1, "ag", 2) != 0)
		    {
		    bed->blockSizes[i] -= 1;
		    bed->chromStarts[i] += 1;
		    }
		else if (memcmp(intronEnd-2+1, "ag", 2) != 0)
		    {
		    bed->blockSizes[i] += 1;
		    bed->chromStarts[i] -= 1;
		    }
		}
	    }
	else
	    {
	    if (memcmp(intronStart, "ct", 2) != 0)
		{
		if (memcmp(intronStart-1, "ct", 2) == 0)
		    {
		    bed->blockSizes[i-1] -= 1;
		    }
		else if (memcmp(intronStart+1, "ct", 2) == 0)
		    {
		    bed->blockSizes[i-1] += 1;
		    }
		}
	    if (memcmp(intronEnd-2, "ac", 2) != 0)
	        {
		if (memcmp(intronEnd-2-1, "ac", 2) != 0)
		    {
		    bed->blockSizes[i] -= 1;
		    bed->chromStarts[i] += 1;
		    }
		else if (memcmp(intronEnd-2+1, "ac", 2) != 0)
		    {
		    bed->blockSizes[i] += 1;
		    bed->chromStarts[i] -= 1;
		    }
		}
	    }
	}
    }
}


void txPsltoBed(char *inPsl, char *outBed)
/* txPsltoBed - Convert a psl to a bed file by projecting it onto it's target 
 * sequence. Optionally merge adjacent blocks and trim to splice sites. */
{
FILE *f = mustOpen(outBed, "w");
struct psl *psl, *pslList  = pslLoadAll(inPsl);
char *chromName = "";
struct nibTwoCache *ntc = NULL;
struct dnaSeq *chrom = NULL;

if (dnaPath)
     ntc = nibTwoCacheNew(dnaPath);

verbose(2, "Loaded %d psls\n", slCount(pslList));

if (ntc)
    slSort(&pslList, pslCmpTarget);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    verbose(3, "Processing %s\n", psl->qName);
    if (ntc)
        {
	if (!sameString(chromName, psl->tName))
	    {
	    dnaSeqFree(&chrom);
	    chrom = nibTwoCacheSeq(ntc, psl->tName);
	    toLowerN(chrom->dna, chrom->size);
	    chromName = psl->tName;
	    verbose(2, "Loaded %d bases in %s\n", chrom->size, chromName);
	    }
	}
    struct bed *bed = pslToBed(psl, mergeMax);
    if (fixIntrons)
        fixBedIntrons(bed, chrom);
    bedTabOutN(bed, 12, f);
    bedFree(&bed);
    }
carefulClose(&f);
dnaSeqFree(&chrom);
nibTwoCacheFree(&ntc);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
mergeMax = optionInt("mergeMax", mergeMax);
fixIntrons = !optionExists("noFixIntrons");
dnaPath = optionVal("dnaPath", dnaPath);
if (fixIntrons && !dnaPath)
    errAbort("dnaPath required with fixIntrons option.");
if (argc != 3)
    usage();
txPsltoBed(argv[1], argv[2]);
return 0;
}
