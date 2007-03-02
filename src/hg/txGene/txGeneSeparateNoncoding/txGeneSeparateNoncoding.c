/* txGeneSeparateNoncoding - Separate genes into three piles - coding, non-coding 
 * that overlap coding, and independent non-coding. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txInfo.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: txGeneSeparateNoncoding.c,v 1.2 2007/03/02 01:39:37 kent Exp $";

int minNearOverlap=20;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneSeparateNoncoding - Separate genes into three piles - coding, non-coding that overlap coding, and independent non-coding.\n"
  "usage:\n"
  "   txGeneSeparateNoncoding in.bed coding.bed nearCoding.bed noncoding.bed\n"
  "where:\n"
  "   in.bed is input transcripts\n"
  "   in.cluster is protein cluster file (from txCdsCluster)\n"
  "   coding.bed contains coding transcripts\n"
  "   nearCoding.bed contains noncoding transcripts overlapping coding transcripts\n"
  "   noncoding.bed contains other noncoding transcripts\n"
  "options:\n"
  "   -minNearOverlap=N - Minimum overlap with coding gene to be considered near-coding\n"
  "                       Default is %d\n"
  , minNearOverlap
  );
}

static struct optionSpec options[] = {
   {"minNearOverlap", OPTION_INT},
   {NULL, 0},
};

void writeBedList(struct bed *bedList, FILE *f)
/* Write all beds in list to file. */
{
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    bedTabOutN(bed, 12, f);
}

void addBedBlocksToRangeTree(struct rbTree *rangeTree, struct bed *bed)
/* Add all blocks of bed to tree. */
{
int i, blockCount=bed->blockCount;
for (i=0; i<blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    rangeTreeAdd(rangeTree, start, end);
    }
}

int bedOverlapWithRangeTree(struct rbTree *rangeTree, struct bed *bed)
/* Return total overlap (at block level) of bed with tree */
{
int total = 0;
int i, blockCount=bed->blockCount;
for (i=0; i<blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    total += rangeTreeOverlapSize(rangeTree, start, end);
    }
return total;
}

void separateOneStrand(struct bed *inList, struct hash *infoHash,
	struct bed **retCoding, struct bed **retNearCoding, struct bed **retNoncoding)
/* Separate bed list into three parts depending on whether or not
 * it's coding. */
{
*retCoding = *retNearCoding = *retNoncoding = NULL;
struct bed *bed, *nextBed;

/* Make one pass through data putting in all of the coding into
 * a range tree. */
struct rbTree *rangeTree = rangeTreeNew();
for (bed = inList; bed != NULL; bed = bed->next)
    {
    if (bed->thickStart < bed->thickEnd)
	addBedBlocksToRangeTree(rangeTree, bed);
    }

/* Make second pass splitting things up. */
for (bed = inList; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    struct txInfo *info = hashMustFindVal(infoHash, bed->name);
    if (bed->thickStart < bed->thickEnd)
	{
	slAddHead(retCoding, bed);
	info->category = "coding";
	}
    else if (bedOverlapWithRangeTree(rangeTree, bed) >= minNearOverlap)
        {
	slAddHead(retNearCoding, bed);
	info->category = "nearCoding";
        }
    else
        {
	slAddHead(retNoncoding, bed);
	info->category = "noncoding";
	}
    }

slReverse(retCoding);
slReverse(retNearCoding);
slReverse(retNoncoding);
rangeTreeFree(&rangeTree);
}

void txGeneSeparateNoncoding(char *inBed, char *inInfo,
	char *outCoding, char *outNearCoding, char *outNoncoding, char *outInfo)
/* txGeneSeparateNoncoding - Separate genes into three piles - coding, 
 * non-coding that overlap coding, and independent non-coding. */
{
/* Read in txInfo into a hash keyed by transcript name */
struct hash *infoHash = hashNew(16);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);
verbose(2, "Read info on %d transcripts from %s\n", infoHash->elCount, 
	inInfo);

/* Read in bed, and sort so we can process it easily a 
 * strand of one chromosome at a time. */
struct bed *inBedList = bedLoadNAll(inBed, 12);
slSort(&inBedList, bedCmpChromStrandStart);

/* Open up output files. */
FILE *fCoding = mustOpen(outCoding, "w");
FILE *fNearCoding = mustOpen(outNearCoding, "w");
FILE *fNoncoding = mustOpen(outNoncoding, "w");

/* Go through input one chromosome strand at a time. */
struct bed *chromStart, *chromEnd = NULL;
for (chromStart = inBedList; chromStart != NULL; chromStart = chromEnd)
    {
    verbose(2, "chrom %s %s\n", chromStart->chrom, chromStart->strand); 

    /* Find chromosome end. */
    char *chromName = chromStart->chrom;
    char strand = chromStart->strand[0];
    struct bed *bed, *dummy, **endAddress = &dummy;
    for (bed = chromStart; bed != NULL; bed = bed->next)
        {
	if (!sameString(bed->chrom, chromName))
	    break;
	if (bed->strand[0] != strand)
	    break;
	endAddress = &bed->next;
	}
    chromEnd = bed;

    /* Terminate list before next chromosome */
    *endAddress = NULL;

    /* Do the separation. */
    struct bed *codingList, *nearCodingList, *noncodingList;
    separateOneStrand(chromStart, infoHash, &codingList, &nearCodingList, &noncodingList);

    /* Write lists to respective files. */
    writeBedList(codingList, fCoding);
    writeBedList(nearCodingList, fNearCoding);
    writeBedList(noncodingList, fNoncoding);
    }
carefulClose(&fCoding);
carefulClose(&fNearCoding);
carefulClose(&fNoncoding);

/* Write out updated info file */
FILE *f = mustOpen(outInfo, "w");
for (info = infoList; info != NULL; info = info->next)
    txInfoTabOut(info, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
minNearOverlap = optionInt("minNearOverlap", minNearOverlap);
txGeneSeparateNoncoding(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
