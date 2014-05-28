/* bedMergeOverlappingBlocks - Fix faulty BED 12 files with illegal overlapping blocks. Also reports a summary of the changes.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "bed.h"
#include "rangeTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedMergeOverlappingBlocks - Fix faulty BED 12 files with illegal overlapping blocks.\n"
  "usage:\n"
  "   bedMergeOverlappingBlocks in.bed out.bed\n"
  "options:\n"
  "   -report=summary.txt      log problems in a file\n"
  );
}

static struct optionSpec options[] = {
   {"report", OPTION_STRING},
   {NULL, 0},
};

void sortAndMergeBlocks(struct bed *oneBed)
/* construct a range tree out of blocks, then remake blocks arrays */
/* this should automatically merge everything. */
{
struct rbTree *rt = rangeTreeNew();
struct range *rangeList = NULL;
struct range *oneRange;
int i;
for (i = 0; i < oneBed->blockCount; i++)
    {
    rangeTreeAdd(rt, oneBed->chromStarts[i], oneBed->chromStarts[i] + oneBed->blockSizes[i]);
    /* delete old (bad) blocks in bed */
    oneBed->chromStarts[i] = oneBed->blockSizes[i] = 0;
    }
rangeList = rangeTreeList(rt);
oneBed->blockCount = slCount(rangeList);
/* remake bed blocks out of range tree */
i = 0;
for (oneRange = rangeList; oneRange != NULL; oneRange = oneRange->next)
    {
    oneBed->chromStarts[i] = oneRange->start;
    oneBed->blockSizes[i] = oneRange->end - oneRange->start;
    i++;
    }
rangeTreeFree(&rt);
}

int fixBed(struct bed *oneBed, int lineNum, FILE *log)
/* Change bed in place, and log change if made. Return 1 if changed. */ 
{
int i;
boolean unsorted = FALSE;
boolean overlapping = FALSE;
/* Check if it's sorted */
for (i = 0; i < oneBed->blockCount-1; i++)
    if (oneBed->chromStarts[i] >= oneBed->chromStarts[i+1])
	{
	unsorted = TRUE;
	if (log)
	    fprintf(log, "line %d: unsorted blocks detected.  %d >= %d\n", 
		    lineNum, oneBed->chromStarts[i], oneBed->chromStarts[i+1]);
	}
/* Check for overlap. */
for (i = 0; i < oneBed->blockCount-1; i++)
    {
    int endI = oneBed->chromStarts[i] + oneBed->blockSizes[i];
    if (endI > oneBed->chromStarts[i+1])
	{
	overlapping = TRUE;
	if (log)
	    fprintf(log, "line %d: overlapping blocks detected. %d + %d > %d\n",
		    lineNum, oneBed->chromStarts[i], oneBed->blockSizes[i], 
		    oneBed->chromStarts[i+1]);
	}
    }
/* Go through with it only if we have to. */
if (unsorted || overlapping)
    {
    sortAndMergeBlocks(oneBed);
    return 1;
    }
return 0;
}

void bedMergeOverlappingBlocks(char *inBed, char *outBed)
/* bedMergeOverlappingBlocks - Fix faulty BED 12 files with illegal overlapping blocks. Also reports a summary of the changes.. */
{
int badBeds = 0;
FILE *log = NULL;
FILE *newBedFile = mustOpen(outBed, "w");
char *logName = optionVal("report", NULL);
struct lineFile *lf = lineFileOpen(inBed, TRUE);
char *line, *row[12];
boolean isItemRgb = FALSE;
if (logName)
    log = mustOpen(logName, "w");
while (lineFileNext(lf, &line, NULL))
    {
    struct bed *bed;
    int numFields = chopByWhite(line, row, ArraySize(row));
    /* strange it's reading empty lines... whatever */
    if (numFields == 0)
	continue;
    if (numFields < 12)
	errAbort("file %s doesn't appear to be in blocked-bed format. At least 12 fields required, got %d", inBed, numFields);
    if (bedParseRgb(row[8]))
	isItemRgb = TRUE;
    bed = bedLoadN(row, numFields);
    badBeds += fixBed(bed, lf->lineIx, log);
    if (isItemRgb)
	bedTabOutNitemRgb(bed, numFields, newBedFile);
    else
	bedTabOutN(bed, numFields, newBedFile);
    }
lineFileClose(&lf);
if (log)
    {
    fprintf(log, "Fixed %d bad beds in all.\n", badBeds);
    carefulClose(&log);
    }
carefulClose(&newBedFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedMergeOverlappingBlocks(argv[1], argv[2]);
return 0;
}
