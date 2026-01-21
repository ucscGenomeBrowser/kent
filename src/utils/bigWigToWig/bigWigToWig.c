/* bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the 
 * original wig than bigWigToBedGraph does, but still will break up large stepped sections into 
 * smaller ones. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "udc.h"
#include "bigWig.h"
#include "obscure.h"
#include "basicBed.h"
#include "bigBedCmdSupport.h"


char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;	/* Bed file that specifies bounds of sequences. */
char *clPos = NULL;	/* Positions file that specifies bounds of sequences. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the\n"
  "original wig than bigWigToBedGraph does, but still will break up large stepped sections\n"
  "into smaller ones.\n"
  "usage:\n"
  "   bigWigToWig in.bigWig out.wig\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restict output to only that under end\n"
  "   -bed=input.bed  Extract values for all ranges specified by input.bed. If bed4, will also print the bed name.\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"udcDir", OPTION_STRING},
   {"bed", OPTION_STRING},
   {"positions", OPTION_STRING},
   {NULL, 0},
};




static void processChromChunk(struct bbiFile *bbi, char *chrom,
                              int start, int end, char *bedName, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
{
verbose(2, "==> extract %s:%d-%d   %s\n", chrom, start, end, bedName);
if (bedName)
    bigWigIntervalDumpWithName(bbi, chrom, start, end, 0, bedName, f); 
else
    bigWigIntervalDump(bbi, chrom, start, end, 0, f);
}


void bigWigToWig(char *inFile, char *outFile)
/* bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the 
 * original wig than bigWigToBedGraph does, but still will break up large stepped sections into 
 * smaller ones. */
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
    char *chromName = chrom->name;
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
    bigWigIntervalDump(bbi, chromName, start, end, 0, f);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bbi);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clBed = optionVal("bed", clBed);
clPos = optionVal("positions", clPos);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if ((clBed || clPos) && (clChrom || (clStart >= 0) || (clEnd >= 0)))
    errAbort("-bed or -positions can not be used with -chrom -start or -end options");
if (clBed && clPos)
    errAbort("-bed and -positions can not be used together");

bigWigToWig(argv[1], argv[2]);

if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
