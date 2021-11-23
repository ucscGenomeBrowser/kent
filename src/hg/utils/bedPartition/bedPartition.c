/* bedPartition - split BED ranges into non-overlapping ranges  */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "options.h"
#include "partitionSort.h"
#include "basicBed.h"
#include "sqlNum.h"
#include "dystring.h"
#include "portable.h"


/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"partSize", OPTION_INT},
    {"parallel", OPTION_INT},
    {NULL, 0}
};
static int gPartSize = 1;
static int gParallel = 0;

static void usage()
/* Explain usage and exit. */
{
errAbort("bedPartition - split BED ranges into non-overlapping ranges\n"
  "usage:\n"
  "   bedPartition [options] bedFile rangesBed\n"
  "\n"
  "Split ranges in a BED into non-overlapping sets for use in cluster jobs.\n"
  "Output is a BED 3 of the ranges.\n"
  "The bedFile maybe compressed and no ordering is assumed.\n"
  "\n"
  "options:\n"
  "   -partSize=1 - will combine non-overlapping partitions, up to\n"
  "    this number of ranges.\n"
  "    per set of overlapping records.\n"
  "   -parallel=n - use this many cores for parallel sorting\n");
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


static struct bed3 *readPartition(struct bedInput *bi)
/* read next set of overlapping beds */
{
struct bed3 *bedPart = bedInputNext(bi);
struct bed3 *bed;

if (bedPart == NULL)
    return NULL;  /* no more */
/* add more beds while they overlap, due to way input is sorted
 * with by start and then reverse end */

int partCnt = 1; // already have one
while ((bed = bedInputNext(bi)) != NULL)
    {
    boolean samePart = isOverlapped(bed, bedPart);
    if (samePart || ((partCnt < gPartSize) && sameChrom(bed, bedPart)))
        {
        bedPart->chromStart = min(bedPart->chromStart, bed->chromStart);
        bedPart->chromEnd = max(bedPart->chromEnd, bed->chromEnd);
        bed3Free(&bed);
        if (!samePart)
            partCnt++;
        }
    else
        {
        bedInputPutBack(bi, bed);
        break;
        }
    }
return bedPart;
}

static void bedPartition(char *bedFile, char *rangesBed)
/* split BED files into non-overlapping sets */
{
struct bedInput *bi = bedInputNew(bedFile);
struct bed3 *bedPart;
FILE *outFh = mustOpen(rangesBed, "w");

while ((bedPart = readPartition(bi)) != NULL)
    {
    fprintf(outFh, "%s\t%d\t%d\n", bedPart->chrom, bedPart->chromStart, bedPart->chromEnd);
    bed3Free(&bedPart);
    }
carefulClose(&outFh);
bedInputFree(&bi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
gPartSize = optionInt("partSize", gPartSize);
gParallel = optionInt("parallel", gParallel);
bedPartition(argv[1], argv[2]);
return 0;
}
