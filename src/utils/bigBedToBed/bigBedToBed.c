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
boolean header = FALSE;
boolean tsv = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigBedToBed v1 - Convert from bigBed to ascii bed format.\n"
  "usage:\n"
  "   bigBedToBed input.bb output.bed\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restrict output to only that under end\n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
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
   {"udcDir", OPTION_STRING},
   {"header", OPTION_BOOLEAN},
   {"tsv", OPTION_BOOLEAN},
   {NULL, 0},
};

static void writeFeatures(struct bbiFile *bbi, char *chromName, int start, int end, int itemsLeft, struct lm *lm, FILE *f)
{
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

static void bigBedToBedFromBed(struct bbiFile *bbi, char *bedFileName, FILE *outFile)
{
struct bed *bed, *bedList = bedLoadNAll(bedFileName, 3);
int itemsLeft = 0;
struct lm *lm = lmInit(0);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    writeFeatures(bbi, bed->chrom, bed->chromStart, bed->chromEnd, itemsLeft, lm, outFile);
    }
lmCleanup(&lm);
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
    bigBedToBedFromBed(bbi, clBed, f);
    return;
    }

struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;
    char *chromName = chrom->name;
    int start = 0, end = chrom->size;
    if (clStart > 0)
        start = clStart;
    if (clEnd > 0)
        end = clEnd;
    int itemsLeft = 0;	// Zero actually means no limit.... 
    struct lm *lm = lmInit(0);
    writeFeatures(bbi, chromName, start, end, itemsLeft, lm, f);
    lmCleanup(&lm);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bbi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clBed = optionVal("bed", clBed);
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
header = optionExists("header");
tsv = optionExists("tsv");
if (header & tsv)
    errAbort("can't specify both -header and -tsv");
if (argc != 3)
    usage();
bigBedToBed(argv[1], argv[2]);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
