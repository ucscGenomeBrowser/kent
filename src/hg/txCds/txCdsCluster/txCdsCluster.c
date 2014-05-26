/* txCdsCluster - Cluster transcripts purely in the CDS regions, only putting things together 
 * if they share same frame as well as a genomic region. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "binRange.h"
#include "rangeTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsCluster - Cluster transcripts purely in the CDS regions, only putting things\n"
  "together if they share same frame as well as a genomic region.\n"
  "usage:\n"
  "   txCdsCluster input.bed output.clusters\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct txCdsCluster
/* A cluster of overlapping (at the block level on the same strand)
 * alignments. */
    {
    struct txCdsCluster *next;
    int start,end;
    char strand;
    struct bed *bedList;
    struct rbTree *frameTrees[3];
    };

void txCdsClusterFree(struct txCdsCluster **pCluster)
/* Free up txCdsCluster */
{
struct txCdsCluster *cluster = *pCluster;
if (cluster != NULL)
    {
    bedFreeList(&cluster->bedList);
    rbTreeFree(&cluster->frameTrees[0]);
    rbTreeFree(&cluster->frameTrees[1]);
    rbTreeFree(&cluster->frameTrees[2]);
    freez(pCluster);
    }
}

void txCdsClusterFreeList(struct txCdsCluster **pList)
/* Free a list of dynamically allocated txCdsClusters */
{
struct txCdsCluster *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    txCdsClusterFree(&el);
    }
*pList = NULL;
}

void txCdsClusterSave(struct txCdsCluster *cluster, int id, FILE *f)
/* Write out info on cluster to file. */
{
struct bed *bed = cluster->bedList;
fprintf(f, "%s\t%d\t%d\t", bed->chrom, cluster->start, cluster->end);
fprintf(f, "tcc%d\t", id);
fprintf(f, "0\t%c\t", cluster->strand);
fprintf(f, "%d\t", slCount(cluster->bedList));
for (bed=cluster->bedList; bed != NULL; bed = bed->next)
    fprintf(f, "%s,", bed->name);
fprintf(f, "\n");
}

struct txCdsCluster *txCdsClusterNew(struct bed *bed, 
	struct lm *lm, struct rbTreeNode *rbStack[128])
/* Create cluster around a single bed. */
{
/* Allocate and fill in basic fields. */
struct txCdsCluster *cluster;
AllocVar(cluster);
cluster->start = bed->thickStart;
cluster->end = bed->thickEnd;
cluster->bedList = bed;
bed->next = NULL;

/* Allocate frame trees. */
int i;
for (i=0; i<3; ++i)
    cluster->frameTrees[i] = rangeTreeNewDetailed(lm, rbStack);

/* Fill in frame trees with coding exons. */
int cdsPos = 0;
int block, blockCount = bed->blockCount;
for (block = 0; block < blockCount; ++block)
    {
    int start = bed->chromStart + bed->chromStarts[block];
    int end = start + bed->blockSizes[block];
    start = max(start, bed->thickStart);
    end = min(end, bed->thickEnd);
    if (start < end)
	{
	int size = end - start;
	int frame = (start - cdsPos)%3;
	rangeTreeAdd(cluster->frameTrees[frame], start, end);
	cdsPos += size;
	}
    }
return cluster;
}

void txCdsClusterMerge(struct txCdsCluster *a, struct txCdsCluster **pB)
/* Merge b into a.  Destroys b. */
{
struct txCdsCluster *b = *pB;
a->bedList = slCat(a->bedList, b->bedList);
b->bedList = NULL;
a->start = min(a->start, b->start);
a->end = max(a->end, b->end);
struct range *range;
int frame;
for (frame=0; frame<3; ++frame)
    {
    struct rbTree *aTree = a->frameTrees[frame];
    struct rbTree *bTree = b->frameTrees[frame];
    for (range = rangeTreeList(bTree); range != NULL; range = range->next)
	{
	rangeTreeAdd(aTree, range->start, range->end);
	}
    freez(&b->frameTrees[frame]);  /* Needed here, so txCdsClusterFree doesn't do it. */
    }
txCdsClusterFree(pB);
}

boolean bedIntersectsCluster(struct txCdsCluster *cluster, struct bed *bed)
/* Return TRUE if any block in bed intersects with cluster. */
{
int block, blockCount = bed->blockCount;
int cdsPos = 0;
for (block = 0; block < blockCount; ++block)
    {
    struct range tempR;
    int start = bed->chromStarts[block] + bed->chromStart;
    int end = start + bed->blockSizes[block];
    start = max(start, bed->thickStart);
    end = min(end, bed->thickEnd);
    if (start < end)
	{
	int size = end - start;
	int frame = (start - cdsPos)%3;
	tempR.start = start;
	tempR.end = end;
	if (rbTreeFind(cluster->frameTrees[frame], &tempR))
	    return TRUE;
	cdsPos += size;
	}
    }
return FALSE;
}

void flipBed(struct bed *bed, int chromSize)
/* Flip bed to seem to be on other strand. */
{
int bedSize = bed->chromEnd - bed->chromStart;
bed->strand[0] = (bed->strand[0] == '+' ? '-' : '+');
reverseUnsignedRange(&bed->chromStart, &bed->chromEnd, chromSize);
reverseUnsignedRange(&bed->thickStart, &bed->thickEnd, chromSize);

/* Convert blockSizes for moment to hold ends. */
int i, blockCount = bed->blockCount;
for (i=0; i<blockCount; ++i)
    bed->blockSizes[i] += bed->chromStarts[i];

/* Flip start and end. */
for (i=0; i<blockCount; ++i)
    reverseIntRange(&bed->chromStarts[i], &bed->blockSizes[i], bedSize);

/* Convert blockSizes from ends back to sizes. */
for (i=0; i<blockCount; ++i)
    bed->blockSizes[i] -= bed->chromStarts[i];

/* Reverse arrays */
reverseInts(bed->blockSizes, blockCount);
reverseInts(bed->chromStarts, blockCount);
}

void flipBedList(struct bed *bedList, int chromSize)
/* Flip a list of beds. */
{
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
     flipBed(bed, chromSize);
}

void bedIntoBinsOfClusters(struct bed *bed, struct binKeeper *bins, 
	struct lm *lm, struct rbTreeNode *rbStack[128])
/* Add lb into a binKeeper full of txCdsClusters.  This will create and merge 
 * clusters as need be. */
{
/* Create new cluster around bed. */
struct txCdsCluster *newCluster = txCdsClusterNew(bed, lm, rbStack);

/* Merge in any existing overlapping clusters.. */
struct binElement *bel, *belList = binKeeperFind(bins, bed->thickStart, bed->thickEnd);
for (bel = belList; bel != NULL; bel = bel->next)
    {
    struct txCdsCluster *oldCluster = bel->val;
    if (bedIntersectsCluster(oldCluster, bed))
	{
	binKeeperRemove(bins, oldCluster->start, oldCluster->end, oldCluster);
	txCdsClusterMerge(oldCluster, &newCluster);
	newCluster = oldCluster;
	}
    }
slFreeList(&belList);

/* Add to binKeeper. */
binKeeperAdd(bins, newCluster->start, newCluster->end, newCluster);
}

struct txCdsCluster *clusterOneStrand(struct bed *bedList)
/* Make clusters of overlapping beds on a single chromosome. 
 * Return a list of such clusters. */
{
/* Set up local memory pool and stack for all the rangeTrees we'll be using. */
struct lm *lm = lmInit(0);
struct rbTreeNode *rbStack[128];
boolean minusStrand = (bedList->strand[0] == '-');


/* Figure out maximum size needed for binKeeper */
struct bed *bed, *nextBed;
int strandSizeLimit = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    strandSizeLimit = max(strandSizeLimit, bed->chromEnd);

/* Flip around beds if on minus strand. */
if (minusStrand)
    flipBedList(bedList, strandSizeLimit);

/* Create binKeeper full of clusters. */
struct binKeeper *bins = binKeeperNew(0, strandSizeLimit);
for (bed = bedList; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    if (bed->thickStart < bed->thickEnd)
	bedIntoBinsOfClusters(bed, bins, lm, rbStack);
    }

/* Convert from binKeeper of clusters to simple list of clusters. */
struct txCdsCluster *clusterList = NULL;
struct binElement *binList, *bin;
binList = binKeeperFindAll(bins);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct txCdsCluster *cluster = bin->val;
    if (minusStrand)
	{
	cluster->strand = '-';
        reverseIntRange(&cluster->start, &cluster->end, strandSizeLimit);
	}
    else
        cluster->strand = '+';
    slAddHead(&clusterList, cluster);
    }
slReverse(&clusterList);

/* Flip around beds if on minus strand. */
if (minusStrand)
    flipBedList(bedList, strandSizeLimit);

/* Clean up and go home. */
struct txCdsCluster *cluster;
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    freez(&cluster->frameTrees[0]);
    freez(&cluster->frameTrees[1]);
    freez(&cluster->frameTrees[2]);
    }
lmCleanup(&lm);
binKeeperFree(&bins);
return clusterList;
}

void txCdsCluster(char *inFile, char *outFile)
/* txCdsCluster - Cluster transcripts purely in the CDS regions, only putting things 
 * together if they share same frame as well as a genomic region. */
{
/* Load input and sort by chromosome. */
struct bed *bedList = bedLoadNAll(inFile, 12);
slSort(&bedList, bedCmpChromStrandStart);

/* Open output and initialize unique ID generator. */
FILE *f = mustOpen(outFile, "w");
int clusterId = 0;

/* Go through input one chromosome strand at a time. */
struct bed *chromStart, *chromEnd = NULL;
for (chromStart = bedList; chromStart != NULL; chromStart = chromEnd)
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

    /* Create clusters. */
    struct txCdsCluster *clusterList = clusterOneStrand(chromStart);

    /* Write clusters to file. */
    struct txCdsCluster *cluster;
    for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
         txCdsClusterSave(cluster, ++clusterId, f);

    txCdsClusterFreeList(&clusterList);	/* Note free's beds as well! */
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txCdsCluster(argv[1], argv[2]);
return 0;
}
