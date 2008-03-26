/* txBedToGraph - Cluster together beds from txPslToBed. Make transcript graphs out of clusters.
 * The overall flow  of the algorithm is:
 *   Read in input, and separate it groups by strand and chromosome.
 *   For each chromosome strand:
 *       Cluster input that overlaps at the exon level.
 *       Turn each cluster into a graph.
 * This module handles i/o and clustering.  The makeGraph module
 * handles the graph building. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ggTypes.h"
#include "bed.h"
#include "dnautil.h"
#include "rangeTree.h"
#include "binRange.h"
#include "txGraph.h"
#include "nibTwo.h"
#include "txBedToGraph.h"

int maxJoinSize = 70000;	/* This excludes most of the chr14 IG mess */
boolean forceRefSeqJoin = TRUE;
int maxBleedOver = 6;
int maxUncheckedBleed = 3;
struct nibTwoCache *seqCache = NULL;
char *prefix = "a";
double singleExonMaxOverlap = 0.60;

boolean trustedSource(char *sourceType)
/* Return TRUE source type is trusted (refSeq or something similar). */ 
{
return sameString(sourceType, "refSeq") || sameString(sourceType, "ccds");
}

boolean noCutSource(char *sourceType)
/* Return TRUE if source is not to be truncated during snap to operation. */
{
return sameString(sourceType, "ccds");
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txBedToGraph - Cluster together beds from txPslToBed. Make transcript graphs out of clusters.\n"
  "   txBedToGraph in1.bed in1Type [in2.bed in2type ...]  out.txg\n"
  "options:\n"
  "    -maxJoinSize=N - Maximum size of gap to join for introns with\n"
  "                    noisy ends. Default %d.\n"
  "    -noForceRefSeqJoin - If set don't force joins above maxJoinSize\n"
  "                    on refSeq type\n"
  "    -maxBleedOver=N - Maximum amount of exon that can be lost when snapping\n"
  "                    soft to hard edges. Default %d\n"
  "    -maxUncheckedBleed=N - Maximum amount of exon that can be lost when\n"
  "                    snapping soft to hard edges without checking nucleotide\n"
  "                    match. Only used checkSeq is set.  Default %d\n"
  "    -checkSeq=file.2bit - If true check sequence when snapping off ends. Can use nib\n"
  "                    dir or two bit file.\n"
  "    -prefix=xyz - Use the given prefix for the graph names, default %s\n"
  "    -singleExonMaxOverlap=0.N - Maximum ratio of single exon that can overlap\n"
  "                                a multi-exon gene.  Default %g\n"
  , maxJoinSize, maxBleedOver, maxUncheckedBleed, prefix, singleExonMaxOverlap
  );
}

static struct optionSpec options[] = {
   {"maxJoinSize", OPTION_INT},
   {"noForceRefSeqJoin", OPTION_BOOLEAN},
   {"maxBleedOver", OPTION_INT},
   {"maxUncheckedBleed", OPTION_INT},
   {"checkSeq", OPTION_STRING},
   {"prefix", OPTION_STRING},
   {"singleExonMaxOverlap", OPTION_FLOAT},
   {NULL, 0},
};


int linkedBedsCmpChromAndStrand(const void *va, const void *vb)
/* Compare to sort based on chrom,strand,chromStart. */
{
const struct linkedBeds *a = *((struct linkedBeds **)va);
const struct linkedBeds *b = *((struct linkedBeds **)vb);
struct bed *aBed = a->bedList, *bBed = b->bedList;
int dif;
dif = strcmp(aBed->chrom, bBed->chrom);
if (dif == 0)
    dif = strcmp(aBed->strand, bBed->strand);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

struct linkedBeds *linkedBedsLoad(char *fileName, char *sourceType)
/* Read a bed file and turn it into a list of linked beds. */
{
struct bed *bedList = bedLoadNAll(fileName, 12);
struct bed *startBed, *bed, *endBed;
struct linkedBeds *lb, *lbList = NULL;
boolean bigGapOk = (sameString(sourceType, "refSeq") && forceRefSeqJoin);
for (startBed = bedList; startBed != NULL; startBed = endBed)
    {
    /* Figure out first bed with different name, or that otherwise can't be
     * a continuation of previous bed. */
    struct bed *lastBed = startBed;
    for (bed = startBed->next; bed != NULL; bed = bed->next)
        {
	if ( lastBed->chromEnd > bed->chromStart || !sameString(lastBed->name, bed->name) 
		|| bed->strand[0] != lastBed->strand[0] 
		|| !sameString(bed->chrom, lastBed->chrom) )
	    {
	    break;
	    }
	if (!bigGapOk)
	    {
	    break;
	    }
	lastBed = bed;
	}
    lastBed->next = NULL;
    endBed = bed;

    /* Create linkedBed and hang it on list. */
    AllocVar(lb);
    lb->bedList = startBed;
    lb->sourceType = sourceType;
    lb->chromStart = startBed->chromStart;
    lb->chromEnd = lastBed->chromEnd;
    slAddHead(&lbList, lb);
    }
slReverse(&lbList);
return lbList;
}

struct lbCluster
/* A cluster of overlapping (at the block level on the same strand)
 * linkedBeds. */
    {
    struct lbCluster *next;
    int chromStart,chromEnd;	/* Bounds of cluster. */
    struct linkedBeds *lbList;	/* Contents of cluster. */
    struct rbTree *exonTree;	/* Tree just used during creation. */
    };

void lbClusterFree(struct lbCluster **pCluster)
/* Free up memory associated with cluster. */
{
/* Note this doesn't free up the linkedBeds. The program
 * has to have all the linkedBeds in memory at one point
 * anyway, and I'm a little lazy right now. */
struct lbCluster *cluster = *pCluster;
if (cluster != NULL)
    {
    freeMem(cluster->exonTree);
    freez(pCluster);
    }
}

struct lbCluster *lbClusterNew(struct linkedBeds *lb, 
	struct lm *lm, struct rbTreeNode *rbStack[128])
/* Create cluster around a single alignment. */
{
/* Allocate and fill in basic fields. */
struct lbCluster *cluster;
AllocVar(cluster);
cluster->chromStart = lb->chromStart;
cluster->chromEnd = lb->chromEnd;
cluster->lbList = lb;
lb->next = NULL;

/* Fill in range tree with exons. */
cluster->exonTree = rangeTreeNewDetailed(lm, rbStack);
struct bed *bed;
for (bed = lb->bedList; bed != NULL; bed = bed->next)
    {
    int block, blockCount = bed->blockCount;
    for (block = 0; block < blockCount; ++block)
        {
	int start = bed->chromStart + bed->chromStarts[block];
	int end = start + bed->blockSizes[block];
	rangeTreeAdd(cluster->exonTree, start, end);
	}
    }
return cluster;
}

void lbClusterMerge(struct lbCluster *a, struct lbCluster **pB)
/* Merge b into a.  Destroys b. */
{
struct lbCluster *b = *pB;
a->lbList = slCat(a->lbList, b->lbList);
b->lbList = NULL;
a->chromStart = min(a->chromStart, b->chromStart);
a->chromEnd = max(a->chromEnd, b->chromEnd);
struct range *range;
for (range = rangeTreeList(b->exonTree); range != NULL; range = range->next)
    rangeTreeAdd(a->exonTree, range->start, range->end);
lbClusterFree(pB);
}

boolean lbIntersectsCluster(struct lbCluster *cluster, struct linkedBeds *lb)
/* Return TRUE if any block in psl intersects with cluster. */
{
struct bed *bed;
for (bed = lb->bedList; bed != NULL; bed = bed->next)
    {
    int block, blockCount = bed->blockCount;
    for (block = 0; block < blockCount; ++block)
	{
	struct range tempR;
	int start = bed->chromStarts[block] + bed->chromStart;
	int end = start + bed->blockSizes[block];
	tempR.start = start;
	tempR.end = end;
	if (rbTreeFind(cluster->exonTree, &tempR))
	    return TRUE;
	}
    }
return FALSE;
}

void lbIntoBinsOfClusters(struct linkedBeds *lb, struct binKeeper *bins, 
	struct lm *lm, struct rbTreeNode *rbStack[128])
/* Add lb into a binKeeper full of lbClusters.  This will create and merge 
 * clusters as need be. */
{
/* Create new cluster around lb. */
struct lbCluster *newCluster = lbClusterNew(lb, lm, rbStack);

/* Merge in any existing overlapping clusters.. */
struct binElement *bel, *belList = binKeeperFind(bins, lb->chromStart, lb->chromEnd);
for (bel = belList; bel != NULL; bel = bel->next)
    {
    struct lbCluster *oldCluster = bel->val;
    if (lbIntersectsCluster(oldCluster, lb))
	{
	binKeeperRemove(bins, oldCluster->chromStart, oldCluster->chromEnd, oldCluster);
	lbClusterMerge(oldCluster, &newCluster);
	newCluster = oldCluster;
	}
    }
slFreeList(&belList);

/* Add to binKeeper. */
binKeeperAdd(bins, newCluster->chromStart, newCluster->chromEnd, newCluster);
}

struct lbCluster *clusterOneStrand(struct linkedBeds *lbList, int strandSizeLimit)
/* Make clusters of overlapping beds on a single chromosome. 
 * Return a list of such clusters. */
{
/* Set up local memory pool and stack for all the rangeTrees we'll be using. */
struct lm *lm = lmInit(0);
struct rbTreeNode *rbStack[128];

/* Create binKeeper full of clusters. */
struct binKeeper *bins = binKeeperNew(0, strandSizeLimit);
struct linkedBeds *lb, *nextLb;
for (lb = lbList; lb != NULL; lb = nextLb)
    {
    nextLb = lb->next;
    lbIntoBinsOfClusters(lb, bins, lm, rbStack);
    }

/* Convert from binKeeper of clusters to simple list of clusters. */
struct lbCluster *clusterList = NULL;
struct binElement *binList, *bin;
binList = binKeeperFindAll(bins);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct lbCluster *cluster = bin->val;
    slAddHead(&clusterList, cluster);
    }
slReverse(&clusterList);

/* Clean up and go home. */
struct lbCluster *cluster;
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    freez(&cluster->exonTree);
lmCleanup(&lm);
binKeeperFree(&bins);
return clusterList;
}

struct txGraph *graphOneStrand(struct linkedBeds *lbList, int strandSizeLimit)
/* Create a list of graphs based on lbList, which is already
 * sorted and restricted to a single strand of a single chromosome. 
 * The interval 0 to strandSizeLimit needs to encompass all the 
 * exons. */
{
struct lbCluster *clusterList = clusterOneStrand(lbList, strandSizeLimit);
struct lbCluster *cluster, *nextCluster;
struct txGraph *graphList = NULL;
for (cluster = clusterList; cluster != NULL; cluster = nextCluster)
    {
    char name[128];
    static int id=0;
    nextCluster = cluster->next;
    safef(name, sizeof(name), "%s%d", prefix, ++id);
    verbose(2, "Got cluster of %d called %s.\n", slCount(cluster->lbList), name);
    struct txGraph *graph = makeGraph(cluster->lbList, maxBleedOver, maxUncheckedBleed, 
    	seqCache, singleExonMaxOverlap, name);
    slAddHead(&graphList, graph);
    lbClusterFree(&cluster);
    }
slReverse(&graphList);
return graphList;
}

void txBedToGraph(char *outGraph, char **inPairs, int inCount)
/* txBedToGraph - Cluster together beds from txPslToBed. Make transcript graphs out of clusters.. */
{
/* Load all bed files and wrap them in sourcedBeds. */
struct linkedBeds *lbList = NULL;
int i;
for (i=0; i<inCount; ++i)
    {
    char *inBed = *inPairs++;
    char *type = *inPairs++;
    verbose(2, "Loading %s\n", inBed);
    lbList = slCat(lbList, linkedBedsLoad(inBed, type));
    }
verbose(2, "Created %d linkedBeds\n", slCount(lbList));

/* Sort and set up loop that processes all input on the same strand
 * of the same chromosome at once. */
slSort(&lbList, linkedBedsCmpChromAndStrand);
struct linkedBeds *strandStart, *strandEnd = NULL;
FILE *f = mustOpen(outGraph, "w");
for (strandStart = lbList; strandStart != NULL; strandStart = strandEnd)
    {
    /* Find chromosome end. */
    char *chromName = strandStart->bedList->chrom;
    char strand = strandStart->bedList->strand[0];
    struct linkedBeds *lb, *dummy, **endAddress = &dummy;
    int strandSizeLimit = 0;
    for (lb = strandStart; lb != NULL; lb = lb->next)
        {
	if (!sameString(lb->bedList->chrom, chromName))
	    break;
	if (lb->bedList->strand[0] != strand)
	    break;
	endAddress = &lb->next;
	strandSizeLimit = max(strandSizeLimit, lb->chromEnd);
	}
    strandEnd = lb;

    /* Terminate list before next chromosome */
    *endAddress = NULL;

    /* Create and write and free graphs. */
    verbose(2, "creating graphs on %s %s\n", 
    	strandStart->bedList->chrom, strandStart->bedList->strand); 
    struct txGraph *graph, *graphList = graphOneStrand(strandStart, strandSizeLimit);
    for (graph = graphList; graph != NULL; graph = graph->next)
        txGraphTabOut(graph, f);
    txGraphFreeList(&graphList);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(500000000);
dnaUtilOpen();
optionInit(&argc, argv, options);
maxJoinSize = optionInt("maxJoinSize", maxJoinSize);
forceRefSeqJoin = !optionExists("noForceRefSeqJoin");
maxBleedOver = optionInt("maxBleedOver", maxBleedOver);
singleExonMaxOverlap = optionDouble("singleExonMaxOverlap", singleExonMaxOverlap);
maxUncheckedBleed = optionInt("maxUncheckedBleed", maxUncheckedBleed);
if (optionExists("checkSeq"))
   seqCache = nibTwoCacheNew(optionVal("checkSeq", NULL));
prefix = optionVal("prefix", prefix);
if (argc < 4 || argc%2 != 0)
    usage();
txBedToGraph(argv[argc-1], argv+1, argc/2-1);
return 0;
}
