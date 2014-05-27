/* randFrag - Grab one or more fragments of DNA of a desired length */
/* at random from some where in the genome. Optionally avoid N's.   */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "bed.h"
#include "jksql.h"
#include "hdb.h"
#include "agpGap.h"
#include "bed.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"numFrags", OPTION_INT},
    {"sizeFixed", OPTION_INT},
    {"sizeAtLeast", OPTION_INT},
    {"sizeNoMoreThan", OPTION_INT},
    {"maxGapPerc", OPTION_DOUBLE},
    {"seed", OPTION_DOUBLE},
    {NULL, 0}
};

/* Variables... explained in usage(). */
unsigned numFrags = 1;
unsigned sizeFixed = 1000000;
unsigned sizeAtLeast = 0;
unsigned sizeNoMoreThan = 0;
double maxGapPerc = 0.001;
boolean fixedSize = TRUE;

void usage()
{
errAbort("randFrag - Grab random fragments of DNA.\n"
	 "usage:\n"
	 "  randFrag database output.bed\n"
	 "where \"sequence\" is a 2bit, fasta, nib, or nibDir.\n"
	 "options:\n"
	 "  -numFrags         Number of frags to put in the output.  (default is one).\n"
	 "  -sizeFixed        Size of each fragment should be the same in output.\n"
	 "                    (Overrides -sizeAtLeast/-sizeNoMoreThan).\n"
	 "                    (Default is 1000000).\n"
         "  -sizeAtLeast      Size of each fragment is at least this.\n"
	 "                    (Use with -sizeNoMoreThan).\n"
	 "  -sizeNoMoreThan   Size of each fragment is no more than this.\n"
	 "                    (Use with -sizeAtLeast).\n"
	 "  -maxGapPerc       Max percentage in range [0.0,1.0] of 'N' and 'X' bases\n"
	 "                    in the found fragment.  If it's too high, the program\n"
	 "                    picks another.  (Default 0.001).");
}

boolean twoBedsOverlap(struct bed *first, struct bed *second)
/* Just checks to see if the two beds overlap really quickly. */
{
if (!sameString(first->chrom, second->chrom))
    return FALSE;
if (first->strand[0] != second->strand[0])
    return FALSE;
if ((first->chromStart < second->chromEnd) && (second->chromStart < first->chromEnd))
    return TRUE;
return FALSE;
}

boolean acceptableRange(struct sqlConnection *conn, struct hash *chromHash, struct bed *candBed, struct bed *currentList)
/* Check whether a random region isn't already part of a selected region */
/* and that there aren't too many gaps there. */
{
struct sqlResult *sr = NULL;
struct agpGap gap;
struct bed *loop;
int rowOffset = 0;
int chromSize = hashIntVal(chromHash, candBed->chrom);
int gappedBases = 0;
char **row;
if (candBed->chromEnd > chromSize)
    return FALSE;
for (loop = currentList; loop != NULL; loop = loop->next)
    if (twoBedsOverlap(loop, candBed))
	return FALSE;
/* Add up gaps in the range of the region. */
/* sr = hRangeQuery(conn, "gap", candBed->chrom, candBed->chromStart, candBed->chromEnd, NULL, &rowOffset); */
/* while ((row = sqlNextRow(sr)) != NULL) */
/*     { */
/*     int gapBegin, gapEnd; */
/*     agpGapStaticLoad(row+rowOffset, &gap); */
/*     gapEnd = (gap.chromEnd > candBed->chromEnd) ? candBed->chromEnd : gap.chromEnd; */
/*     gapBegin = (candBed->chromStart > gap.chromStart) ? candBed->chromStart : gap.chromStart; */
/*     gappedBases += gapEnd - gapBegin; */
/*     } */
/* sqlFreeResult(&sr); */
if ((double)gappedBases/(candBed->chromEnd - candBed->chromStart) > maxGapPerc)
    return FALSE;
return TRUE;
}

char *getChrom(struct hash *chromHash, long long genomeSize, long long *position)
/* Given an overall position in genome (range [0,genomeSize)), return chrom */
/* the position is in, and set the position to be the chrom coordinate. */
{
long long runningTotal = 0;
struct hashEl *helList = hashElListHash(chromHash), *hel;
char *chromName = NULL;
for (hel = helList; hel != NULL; hel = hel->next)
    {
    int chromSize = (int)hel->val;
    if ((runningTotal + (long)chromSize) > *position)
	break;
    runningTotal += (long long)chromSize;
    }
hashElFreeList(&helList);
chromName = cloneString(hel->name);
*position -= runningTotal;
return chromName;
}

/* void winnowChromHash(struct hash *chromHash) */
/* /\* Remove chroms/contigs smaller than the random regions. *\/ */
/* { */
/* struct hashEl *list =  */
/* } */

struct bed *getRanges(char *db)
/* Make a bed, region by region, until there's enough. */
{
struct bed *bedList = NULL;
struct hash *chromHash;
struct sqlConnection *conn = hAllocOrConnect(db);
long long genomeSize = 0;
int numFound = 0;
/* Get the chroms and count them up. */
chromHash = hChromSizeHash(db);
if (!chromHash)
    usage();
genomeSize = hashIntSum(chromHash); 
while (numFound < numFrags)
    {
    long long genomePos = (long long)(genomeSize * (rand() / ((double)RAND_MAX + 1)));
    long long chromPos = genomePos;
    int seqSize = (fixedSize) ? sizeFixed : (int)(((double)rand()/RAND_MAX)*(sizeNoMoreThan - sizeAtLeast));
    char *chrom = getChrom(chromHash, genomeSize, &chromPos);
    char bedName[256];
    struct bed *newBed = NULL;
    AllocVar(newBed);
    newBed->chrom = cloneString(chrom);
    newBed->chromStart = (int)chromPos;
    newBed->chromEnd = (int)chromPos + seqSize;
    safef(bedName, ArraySize(bedName), "rand%d", numFound+1);
    newBed->name = cloneString(bedName);
    if (acceptableRange(conn, chromHash, newBed, bedList))
	{
	slAddHead(&bedList, newBed);
	numFound++;
	/* Maybe his should go to stderr. */
	printf("Found %d regions\n", numFound);
	}
    else
	bedFree(&newBed);
    freeMem(chrom);
    }
hFreeConn(&conn);
slReverse(&bedList);
return bedList;
}

int main(int argc, char *argv[])
/* The program */
{
struct bed *list = NULL, *onebed;
FILE *output;
double seed;
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
if (optionExists("sizeAtLeast") && optionExists("sizeNoMoreThan"))
    {
    fixedSize = FALSE;
    sizeAtLeast = optionInt("sizeAtLeast", sizeAtLeast);
    sizeNoMoreThan = optionInt("sizeNoMoreThan", sizeNoMoreThan);
    if (sizeNoMoreThan < sizeAtLeast)
	usage();
    }
else if (optionExists("sizeAtLeast") || optionExists("sizeNoMoreThan"))
    usage();
else
    sizeFixed = optionInt("sizeFixed", sizeFixed);
numFrags = optionInt("numFrags", numFrags);
/* Start the random num generator. */
if ((seed = optionDouble("seed", seed)) != 0)
    srand(seed);
else
    srand(time(NULL));
maxGapPerc = optionDouble("maxGapPerc", maxGapPerc);
list = getRanges(argv[1]);
/* Why isn't there a bedTabOutAll? */
output = mustOpen(argv[2], "w");
for (onebed = list; onebed != NULL; onebed = onebed->next)
    bedTabOut(onebed, output);
carefulClose(&output);
bedFreeList(&list);
return 0;
}
