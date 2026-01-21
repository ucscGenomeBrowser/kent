/* bigWigToBedGraph - Convert from bigWig to bedGraph format.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bigWig.h"
#include "obscure.h"
#include "basicBed.h"
#include "bigBedCmdSupport.h"


char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;
char *clPos = NULL;

struct lm *lm = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigToBedGraph - Convert from bigWig to bedGraph format.\n"
  "usage:\n"
  "   bigWigToBedGraph in.bigWig out.bedGraph\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restict output to only that under end\n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"bed", OPTION_STRING},
   {"positions", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

static void processChromChunk(struct bbiFile *bbi, char *chrom,
                              int start, int end, char *bedName, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
{
boolean firstTime = TRUE;
int saveStart = -1, prevEnd = -1;
double saveVal = -1.0;

char *chromName = chrom;

struct bbiInterval *interval, *intervalList = bigWigIntervalQuery(bbi, chromName, 
    start, end, lm);
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    if (firstTime)
	{
	saveStart = interval->start;
	saveVal = interval->val;
	firstTime = FALSE;
	}
    else
	{
	if (!((prevEnd == interval->start) && (saveVal == interval->val)))
	    {
	    fprintf(f, "%s\t%u\t%u\t%g\n", chromName, saveStart, prevEnd, saveVal);
	    saveStart = interval->start;
	    saveVal = interval->val;
	    }

	}
    prevEnd = interval->end;
    }
if (!firstTime)
    fprintf(f, "%s\t%u\t%u\t%g\n", chromName, saveStart, prevEnd, saveVal);

}

void bigWigToBedGraph(char *inFile, char *outFile)
/* bigWigToBedGraph - Convert from bigWig to bedGraph format.. */
{
struct bbiFile *bbi = bigWigFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
if (clBed != NULL)
    {
    genericBigToNonBigFromBed(bbi, clBed, f, &processChromChunk);
    return;
    }
if (clPos != NULL)
    {
    genericBigToNonBigFromPos(bbi, clPos, f, &processChromChunk);
    return;
    }
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;

    int start = 0, end = chrom->size;
    if (clStart >= 0)
        start = clStart;
    if (clEnd >= 0)
	{
        end = clEnd;
	if (end > chrom->size)
	    end = chrom->size;
	}
    if (start > end)
	errAbort("invalid range, start=%d > end=%d", start, end);

    processChromChunk(bbi, chrom->name, start, end, NULL, f);

    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bbi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
lm = lmInit(0);
optionInit(&argc, argv, options);
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clBed = optionVal("bed", clBed);
clPos = optionVal("positions", clPos);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
if (argc != 3)
    usage();

if ((clBed || clPos) && (clChrom || (clStart >= 0) || (clEnd >= 0)))
    errAbort("-bed or -positions can not be used with -chrom -start or -end options");
if (clBed && clPos)
    errAbort("-bed and -positions can not be used together");

bigWigToBedGraph(argv[1], argv[2]);

lmCleanup(&lm);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
