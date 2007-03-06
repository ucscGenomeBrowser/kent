/* txGeneSeparateNoncoding - Separate genes into four piles - coding, non-coding 
 * that overlap coding, antisense to coding, and independent non-coding. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txInfo.h"
#include "rangeTree.h"

static char const rcsid[] = "$Id: txGeneSeparateNoncoding.c,v 1.3 2007/03/06 22:41:50 kent Exp $";

int minNearOverlap=20;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneSeparateNoncoding - Separate genes into four piles - coding, non-coding that overlap coding, and independent non-coding.\n"
  "usage:\n"
  "   txGeneSeparateNoncoding in.bed in.info coding.bed nearCoding.bed antisense.bed noncoding.bed out.info\n"
  "where:\n"
  "   in.bed is input transcripts\n"
  "   in.info is txInfo file that will have type field updated in out.infon"
  "   coding.bed contains coding transcripts\n"
  "   nearCoding.bed contains noncoding transcripts overlapping coding transcripts on\n"
  "        same strand\n"
  "   antisense.bed contains noncoding transcripts overlapping coding transcripts on\n"
  "        opposite strand\n"
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

struct chrom
/* A chromosome. */
    {
    struct chrom *next;	/* Next in list. */
    char *name;		/* Chromosome name. */
    struct bed *plusList; /* List of elements on plus strand. */
    struct bed *minusList; /* List of elements on minus strand. */
    };

struct chrom *chromsForBeds(struct bed *bedList)
/* Create list of chromosomes and put bed list in it. */
{
struct hash *hash = hashNew(16);
struct chrom *chromList = NULL, *chrom;
struct bed *bed, *nextBed;
for (bed = bedList; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    chrom = hashFindVal(hash, bed->chrom);
    if (chrom == NULL)
        {
	AllocVar(chrom);
	chrom->name = cloneString(bed->chrom);
	slAddHead(&chromList, chrom);
	hashAdd(hash, chrom->name, chrom);
	}
    if (bed->strand[0] == '-')
        slAddHead(&chrom->minusList, bed);
    else
        slAddHead(&chrom->plusList, bed);
    }
slReverse(&chromList);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    slReverse(&chrom->plusList);
    slReverse(&chrom->minusList);
    }
return chromList;
}

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

struct rbTree *codingTree(struct bed *bedList)
/* Return rangeTree for all coding transcripts in list. */
{
struct rbTree *rangeTree = rangeTreeNew();
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->thickStart < bed->thickEnd)
	addBedBlocksToRangeTree(rangeTree, bed);
    }
return rangeTree;
}

void separateStrand(struct bed *inList, struct hash *infoHash,
	struct rbTree *tree, struct rbTree *antitree,
	struct bed **retCoding, struct bed **retNearCoding, 
	struct bed **retAntisense, struct bed **retNoncoding)
/* Separate list of beds from single strand into the 4 categories.
 * The tree covers the coding transcripts for this strand, and the
 * antitree for the opposite strand. */
{
struct bed *bed, *nextBed;
for (bed = inList; bed != NULL; bed = nextBed)
    {
    boolean uglyUgly = sameString(bed->name, "chr16.1212.1.DQ599768");
    if (uglyUgly) uglyf("got you %s. thickStart %d, thickENd %d\n", bed->name, bed->thickStart, bed->thickEnd);

    nextBed = bed->next;
    struct txInfo *info = hashMustFindVal(infoHash, bed->name);
    if (bed->thickStart < bed->thickEnd)
	{
	if (uglyUgly) uglyf("  coding\n");
	slAddHead(retCoding, bed);
	info->category = "coding";
	}
    else
        {
	if (bedOverlapWithRangeTree(tree, bed) >= minNearOverlap)
	    {
	    slAddHead(retNearCoding, bed);
	    info->category = "nearCoding";
	    }
	else if (bedOverlapWithRangeTree(antitree, bed) >= minNearOverlap)
	    {
	    slAddHead(retAntisense, bed);
	    info->category = "antisense";
	    }
	else
	    {
	    slAddHead(retNoncoding, bed);
	    info->category = "noncoding";
	    }
	if (uglyUgly) uglyf("  %s\n", info->category);
	}
    }
}

void separateChrom(struct chrom *chrom, struct hash *infoHash,
	struct bed **retCoding, struct bed **retNearCoding, 
	struct bed **retAntisense, struct bed **retNoncoding)
/* Separate bed list into four parts depending on whether or not
 * it's coding. */
{
*retCoding = *retNearCoding = *retAntisense = *retNoncoding = NULL;

/* Make trees that cover coding on both strands. */
struct rbTree *plusCoding = codingTree(chrom->plusList);
struct rbTree *minusCoding = codingTree(chrom->minusList);

/* Split things up. */
separateStrand(chrom->plusList, infoHash, plusCoding, minusCoding,
	retCoding, retNearCoding, retAntisense, retNoncoding);
separateStrand(chrom->minusList, infoHash, minusCoding, plusCoding,
	retCoding, retNearCoding, retAntisense, retNoncoding);

/* Clean up and go home. */
rbTreeFree(&plusCoding);
rbTreeFree(&minusCoding);
}

void txGeneSeparateNoncoding(char *inBed, char *inInfo,
	char *outCoding, char *outNearCoding, char *outAntisense,
	char *outNoncoding, char *outInfo)
/* txGeneSeparateNoncoding - Separate genes into four piles - coding, 
 * non-coding that overlap coding, antisense to coding, and independent non-coding. */
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
FILE *fAntisense = mustOpen(outAntisense, "w");

/* Go through input one chromosome strand at a time. */
struct chrom *chrom, *chromList = chromsForBeds(inBedList);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    verbose(2, "chrom %s\n", chrom->name); 

    /* Do the separation. */
    struct bed *codingList, *nearCodingList, *antisenseList, *noncodingList;
    separateChrom(chrom, infoHash, &codingList, &nearCodingList, &antisenseList, &noncodingList);
    verbose(2, "%d coding, %d near, %d anti, %d non\n", 
    	slCount(codingList), slCount(nearCodingList), slCount(antisenseList), slCount(noncodingList));

    /* Write lists to respective files. */
    writeBedList(codingList, fCoding);
    writeBedList(nearCodingList, fNearCoding);
    writeBedList(antisenseList, fAntisense);
    writeBedList(noncodingList, fNoncoding);
    }
carefulClose(&fCoding);
carefulClose(&fNearCoding);
carefulClose(&fNoncoding);
carefulClose(&fAntisense);

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
if (argc != 8)
    usage();
minNearOverlap = optionInt("minNearOverlap", minNearOverlap);
txGeneSeparateNoncoding(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
