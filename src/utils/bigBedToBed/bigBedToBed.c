/* bigBedToBed - Convert from bigBed to ascii bed format.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "udc.h"
#include "bigBed.h"
#include "asParse.h"
#include "obscure.h"
#include "bigBedCmdSupport.h"
#include "basicBed.h"


char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;
char *clPos = NULL;
boolean header = FALSE;
boolean tsv = FALSE;

struct lm *lm = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedToBed v1 - Convert from bigBed to ascii bed format.\n"
  "usage:\n"
  "   bigBedToBed input.bb output.bed\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start 0 based coordiate\n"
  "   -end=N - if set, restrict output to only that under end\n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -header - output a autoSql-style header (starts with '#').\n"
  "   -tsv - output a TSV header (without '#').\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"bed", OPTION_STRING},
   {"positions", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {"header", OPTION_BOOLEAN},
   {"tsv", OPTION_BOOLEAN},
   {NULL, 0},
};

static void processChromChunk(struct bbiFile *bbi, char *chromName, int start, int end, char *bedName, FILE *f)
{
int itemsLeft = 0;	// Zero actually means no limit.... 
struct bigBedInterval *interval, *intervalList = bigBedIntervalQuery(bbi, chromName, 
    start, end, itemsLeft, lm);
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    fprintf(f, "%s\t%u\t%u", chromName, interval->start, interval->end);
    char *rest = interval->rest;
    if (rest != NULL)
	fprintf(f, "\t%s\n", rest);
    else
	fprintf(f, "\n");
    }
}

void bigBedToBed(char *inFile, char *outFile)
/* bigBedToBed - Convert from bigBed to ascii bed format.. */
{
struct bbiFile *bbi = bigBedFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
if (header)
    bigBedCmdOutputHeader(bbi, f);
else if (tsv)
    bigBedCmdOutputTsvHeader(bbi, f);

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

boolean chromFound = FALSE;
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;
    chromFound = TRUE;
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
    processChromChunk(bbi, chromName, start, end, NULL, f);
    }
if (clChrom && !chromFound)
    errAbort("specified chrom %s not found in maf", clChrom);
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
header = optionExists("header");
tsv = optionExists("tsv");
if (header & tsv)
    errAbort("can't specify both -header and -tsv");
if (argc != 3)
    usage();

if ((clBed || clPos) && (clChrom || (clStart >= 0) || (clEnd >= 0)))
    errAbort("-bed or -positions can not be used with -chrom -start or -end options");
if (clBed && clPos)
    errAbort("-bed and -positions can not be used together");

bigBedToBed(argv[1], argv[2]);

lmCleanup(&lm);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
