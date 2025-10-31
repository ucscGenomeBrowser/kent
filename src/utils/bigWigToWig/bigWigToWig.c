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


char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;	/* Bed file that specifies bounds of sequences. */

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
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"udcDir", OPTION_STRING},
   {"bed", OPTION_STRING},
   {NULL, 0},
};

void bigWigToWig(char *inFile, char *outFile)
/* bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the 
 * original wig than bigWigToBedGraph does, but still will break up large stepped sections into 
 * smaller ones. */
{
struct bbiFile *bwf = bigWigFileOpen(inFile);
FILE *f = mustOpen(outFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bwf);
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
    bigWigIntervalDump(bwf, chromName, start, end, 0, f);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bwf);
}


struct bed *bedLoad3Plus(char *fileName)
/* Determines how many fields are in a bedFile and load all beds from
 * a tab-separated file.  Dispose of this with bedFreeList(). 
 * Small change by Michael to require only 3 or more fields. Meaning we will accept bed3 
 */
{
struct bed *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[bedKnownFields];

while (lineFileNextReal(lf, &line))
    {
    int numFields = chopByWhite(line, row, ArraySize(row));
    if (numFields < 3)
	errAbort("file %s doesn't appear to be in bed format. At least 3 fields required, got %d", fileName, numFields);
    slAddHead(&list, bedLoadN(row, numFields));
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

void bigWigToWigFromBed(char *inFile, char *bedFileName, char *outFile)
/* bigWigToWig - Convert bigWig to wig.  This will keep more of the same structure of the 
 * original wig than bigWigToBedGraph does, but still will break up large stepped sections into 
 * smaller ones. */
{
struct bbiFile *bwf = bigWigFileOpen(inFile);
struct bed *bed, *bedList = bedLoad3Plus(bedFileName);
slSort(&bedList, bedCmp);

verbose(2, "call bigWigToWigFromBed %s\n", bedFileName);
FILE *f = mustOpen(outFile, "w");

for (bed = bedList; bed != NULL; bed = bed->next) {
    verbose(2, "==> extract %s:%d-%d   %s\n", bed->chrom, bed->chromStart, bed->chromEnd, bed->name);
    bigWigIntervalDumpWithName(bwf, bed->chrom, bed->chromStart, bed->chromEnd, 0, bed->name, f);
}
carefulClose(&f);
bbiFileClose(&bwf);
bedFreeList(&bedList);
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
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (clBed != NULL) {
	bigWigToWigFromBed(argv[1], clBed, argv[2]);
}else{
	bigWigToWig(argv[1], argv[2]);
}
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}
