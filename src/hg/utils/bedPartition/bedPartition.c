/* bedPartition - split BED ranges into non-overlapping ranges  */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include <limits.h>
#include "common.h"
#include "options.h"
#include "verbose.h"
#include "partitionSort.h"
#include "basicBed.h"
#include "sqlNum.h"
#include "dystring.h"
#include "portable.h"


/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"minPartitionItems", OPTION_INT},
    {"partMergeDist", OPTION_INT},
    {"partMergeSize", OPTION_INT},   // obsolete
    {"parallel", OPTION_INT},
    {NULL, 0}
};
static int gMinPartitionItems = 0;
static int gPartMergeDist = 0;
static int gParallel = 0;

static void usage()
/* Explain usage and exit. */
{
errAbort("bedPartition - split BED ranges into non-overlapping ranges\n"
  "usage:\n"
  "   bedPartition [options] bedFile rangesBed\n"
  "\n"
  "Split ranges in a BED into non-overlapping sets for use in cluster jobs.\n"
  "Output is a BED 4 of the ranges and a generated name.\n"
  "The bedFile maybe compressed and no ordering is assumed.\n"
  "\n"
  "options:\n"
  "   -verbose=1 - print statistics if >= 1\n"
  "   -minPartitionItems=0 - minimum number of input items in a partition. Partitions with\n"
  "    fewer items will merged with into subsequent partitions\n"
  "   -partMergeDist=0 - will combine adjacent non-overlapping partitions that are\n"
  "    separated by no more that this number of bases. -partMergeSize is an obsolete\n"
  "    name for this option.\n"
  "   -parallel=n - use this many cores for parallel sorting\n"
  "notes:\n"
  "   - Generate name is useful for identifying partition\n");
}

struct bedInput
/* object to read a bed */
{
    struct pipeline *pl;     /* sorting pipeline */
    struct lineFile *lf;     /* lineFile to pipeline */
    struct bed3 *pending;     /* next bed to read, if not NULL */
};

static struct bedInput *bedInputNew(char *bedFile)
/* create object to read BEDs */
{
struct bedInput *bi;
AllocVar(bi);
bi->pl = partitionSortOpenPipeline(bedFile, 0, 1, 2, gParallel);
bi->lf = pipelineLineFile(bi->pl);

return bi;
}

static void bedInputFree(struct bedInput **biPtr)
/* free bedInput object */
{
struct bedInput *bi = *biPtr;
if (bi != NULL)
    {
    assert(bi->pending == NULL);
    pipelineClose(&bi->pl);
    freez(biPtr);
    }
}

static struct bed3 *bed3Next(struct lineFile *lf)
/* read a BED3 */
{
char *row[3];
if (!lineFileRow(lf, row))
    return NULL;
return bed3New(row[0], sqlUnsigned(row[1]), sqlUnsigned(row[2]));
}


static struct bed3 *bedInputNext(struct bedInput *bi)
/* read next bed */
{
struct bed3 *bed = bi->pending;
if (bed != NULL)
    bi->pending = NULL;
else
    bed = bed3Next(bi->lf);
return bed;
}

static void bedInputPutBack(struct bedInput *bi, struct bed3 *bed)
/* save bed pending slot. */
{
if (bi->pending)
    errAbort("only one bed maybe pending");
bi->pending = bed;
}

static boolean sameChrom(struct bed3 *bed, struct bed3 *bedPart)
/* chrom the same? */
{
return sameString(bed->chrom, bedPart->chrom);
}

static boolean isOverlapped(struct bed3 *bed, struct bed3 *bedPart)
/* determine if a bed is in the partition */
{
return (bed->chromStart < bedPart->chromEnd) && (bed->chromEnd > bedPart->chromStart)
    && sameChrom(bed, bedPart);
}

static boolean shouldMergeAdjacent(struct bed3 *bed, struct bed3 *bedPart)
/* should this be merged with adjacent due to distance */
{
return (bedPart->chromEnd < bed->chromStart) &&
    ((bed->chromStart - bedPart->chromEnd) < gPartMergeDist);
}

static boolean inclInPartation(struct bed3 *bed, struct bed3 *bedPart,
                               int currentItemCnt)
/* should this BED be included in the current partition? */
{
return sameChrom(bed, bedPart) &&
    ((isOverlapped(bed, bedPart) || (currentItemCnt < gMinPartitionItems) ||
      shouldMergeAdjacent(bed, bedPart)));
}

static struct bed3 *readPartition(struct bedInput *bi, int *itemCntP, int *minPartItemsP,
                                  int *maxPartItemsP)
/* read next set of overlapping beds */
{
struct bed3 *bedPart = bedInputNext(bi);
struct bed3 *bed;
int currentItemCnt = 0;

if (bedPart == NULL)
    return NULL;  /* no more */
currentItemCnt++;

/* add more beds while they overlap, due to way input is sorted
 * with by start and then reverse end */
while ((bed = bedInputNext(bi)) != NULL)
    {
    if (inclInPartation(bed, bedPart, currentItemCnt))
        {
        bedPart->chromStart = min(bedPart->chromStart, bed->chromStart);
        bedPart->chromEnd = max(bedPart->chromEnd, bed->chromEnd);
        bed3Free(&bed);
        currentItemCnt++;
        }
    else
        {
        bedInputPutBack(bi, bed);
        break;
        }
    }

(*itemCntP) += currentItemCnt;
*minPartItemsP = min(*minPartItemsP, currentItemCnt);
*maxPartItemsP = max(*maxPartItemsP, currentItemCnt);
return bedPart;
}

static void bedPartition(char *bedFile, char *rangesBed)
/* split BED files into non-overlapping sets */
{
struct bedInput *bi = bedInputNew(bedFile);
struct bed3 *bedPart;
FILE *outFh = mustOpen(rangesBed, "w");
int partCnt = 0;
int itemCnt = 0;
int minPartItems = INT_MAX;
int maxPartItems = 0;
while ((bedPart = readPartition(bi, &itemCnt, &minPartItems, &maxPartItems)) != NULL)
    {
    fprintf(outFh, "%s\t%d\t%d\tP%d\n", bedPart->chrom, bedPart->chromStart, bedPart->chromEnd, partCnt);
    partCnt++;
    bed3Free(&bedPart);
    }
carefulClose(&outFh);
bedInputFree(&bi);
verbose(1, "Number of items: %d\n", itemCnt);
verbose(1, "Number of partitions: %d\n", partCnt);
verbose(1, "Min items per partition: %d\n", minPartItems);
verbose(1, "Max items per partition: %d\n", maxPartItems);
if (partCnt > 0)
    verbose(1, "Mean items per partition: %0.1f\n", (double)itemCnt / (double)partCnt);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
verboseSetLevel(1);
gMinPartitionItems = optionInt("minPartitionItems", gMinPartitionItems);
if (optionExists("partMergeSize"))
    fprintf(stderr, "Note: -partMergeSize is an obsolete name, use -partMergeDist\n");
// if both are specified, partMergeDist wins
gPartMergeDist = optionInt("partMergeSize", gPartMergeDist);
gPartMergeDist = optionInt("partMergeDist", gPartMergeDist);
gParallel = optionInt("parallel", gParallel);
bedPartition(argv[1], argv[2]);
return 0;
}
