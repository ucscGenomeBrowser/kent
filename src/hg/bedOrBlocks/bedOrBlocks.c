/* bedOrBlocks - Create a bed that is the union of all blocks of a list of beds.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "rangeTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedOrBlocks - Create a bed that is the union of all blocks of a list of beds.\n"
  "usage:\n"
  "   bedOrBlocks in.bed out.bed\n"
  "Where in.bed is a file with the beds to merge, and out.bed is the\n"
  "merged bed. The in.bed should contain 12-column (blocked) beds.\n"
  "All beds on the same strand of the same chromosome will be merged.\n"
  "options:\n"
  "   -useName\n"
  );
}

static struct optionSpec options[] = {
   {"useName", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean useName = FALSE;

struct bed *bedFromRangeTree(struct rbTree *rangeTree, char *chrom, char *name, char *strand)
/* Create a bed based on range tree */
{
struct range *range, *rangeList = rangeTreeList(rangeTree);
if (rangeList == NULL)
    return NULL;

/* Figure out overall start and end, and blockCount.  Assumes
 * (rightly) that rangeList is sorted. */
int chromStart = rangeList->start, chromEnd = rangeList->end;
int blockCount = 1;
for (range = rangeList->next; range != NULL; range = range->next)
    {
    chromEnd = range->end;
    blockCount += 1;
    }

struct bed *bed;
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = chromStart;
bed->chromEnd = chromEnd;
bed->name = cloneString(name);
bed->strand[0] = strand[0];
bed->blockCount = blockCount;
AllocArray(bed->blockSizes, blockCount);
AllocArray(bed->chromStarts, blockCount);
int i = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    bed->blockSizes[i] = range->end - range->start;
    bed->chromStarts[i] = range->start - chromStart;
    ++i;
    }
return bed;
}

void doStrand(struct bed *start, struct bed *end, FILE *f)
/* Assuming all beds from start up to end are on same strand,
 * make a merged bed with all their blocks and output it. */
{
struct rbTree *rangeTree = rangeTreeNew();
struct bed *bed;
for (bed = start; bed != end; bed = bed->next)
    bedIntoRangeTree(bed, rangeTree);
bed = bedFromRangeTree(rangeTree, start->chrom, start->name, start->strand);
bedTabOutN(bed, 12, f);
bedFree(&bed);
rangeTreeFree(&rangeTree);
}

void bedOrBlocks(char *inFile, char *outFile)
/* bedOrBlocks - Create a bed that is the union of all blocks of a list of beds.. */
{
struct bed *start, *end, *inList = bedLoadNAll(inFile, 12);
FILE *f = mustOpen(outFile, "w");
if (useName)
    slSort(&inList, bedCmpChromStrandStartName);
else
    slSort(&inList, bedCmpChromStrandStart);
for (start = inList; start != NULL; start = end)
    {
    for (end = start->next; end != NULL; end = end->next)
        {
	if (!sameString(start->chrom, end->chrom))
	    break;
	if (start->strand[0] != end->strand[0])
	    break;
        if (useName && differentString(start->name, end->name))
            break;
        if (useName && start->chromEnd < end->chromStart)
            break;
	}
    doStrand(start, end, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
useName = optionExists("useName");
bedOrBlocks(argv[1], argv[2]);
return 0;
}
