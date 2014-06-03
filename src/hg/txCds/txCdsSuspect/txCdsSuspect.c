/* txCdsSuspect - Flag cases where the CDS prediction is very suspicious, 
 * including CDSs that lie entirely in an intron or in the 3' UTR of another,
 * better looking transcript. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "rangeTree.h"
#include "txInfo.h"
#include "txCluster.h"
#include "txGraph.h"
#include "cdsEvidence.h"



/* Globals set from command line. */
FILE *fNiceProt = NULL;
int minAaOverlap = 10;
int minCdsOverlap;	/* Set to 3x minAaOverlap. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsSuspect - Flag cases where the CDS prediction is very suspicious, including\n"
  "CDSs that lie entirely in an intron or in the 3' UTR of another, better looking\n"
  "transcript.\n"
  "usage:\n"
  "   txCdsSuspect in.bed in.txg in.cluster in.info out.suspect out.info\n"
  "options:\n"
  "   -niceProt=name - Put the 'nice' proteins and what cluster they're in here.\n"
  "   -minAaOverlap=N - minimum number of amino acids to overlap with to consider\n"
  "                     a CDS pick compatible with one of the primary proteins for\n"
  "                     the cluster. Default %d\n"
  , minAaOverlap
  );
}

static struct optionSpec options[] = {
   {"niceProt", OPTION_STRING},
   {NULL, 0},
};

int overlapInSameFrame(struct bed *a, struct bed *b)
/* Return amount of overlap between coding regions (in same frame)
 * between two beds. */
{
int overlap = 0;

/* Allocate range trees for each frame. */
struct rbTree *frameTrees[3];
int frame;
for (frame = 0; frame<3; ++frame)
    frameTrees[frame] = rangeTreeNew();

/* Fill in frame trees with coding exons of a. */
int cdsPos = 0;
int block, blockCount = a->blockCount;
for (block = 0; block < blockCount; ++block)
    {
    int start = a->chromStart + a->chromStarts[block];
    int end = start + a->blockSizes[block];
    start = max(start, a->thickStart);
    end = min(end, a->thickEnd);
    if (start < end)
	{
	int size = end - start;
	int frame = (start - cdsPos)%3;
	rangeTreeAdd(frameTrees[frame], start, end);
	cdsPos += size;
	}
    }

/* Add up overlaps by comparing bed b against frameTrees */
cdsPos = 0;
blockCount = b->blockCount;
for (block = 0; block < blockCount; ++block)
    {
    int start = b->chromStarts[block] + b->chromStart;
    int end = start + b->blockSizes[block];
    start = max(start, b->thickStart);
    end = min(end, b->thickEnd);
    if (start < end)
	{
	int size = end - start;
	int frame = (start - cdsPos)%3;
	overlap += rangeTreeOverlapSize(frameTrees[frame], start, end);
	cdsPos += size;
	}
    }

/* Clean up and go home. */
for (frame = 0; frame<3; ++frame)
    rangeTreeFree(&frameTrees[frame]);
return overlap;
}

boolean allInUtr3(struct bed *bed, int start, int end)
/* Return TRUE if interval start-end lies entirely in final
 * exon's UTR region. */
{
int utrStart, utrEnd;
if (bed->strand[0] == '+')
    {
    int ix = bed->blockCount-1;
    utrStart = bed->chromStart + bed->chromStarts[ix];
    utrEnd = utrStart + bed->blockSizes[ix];
    utrStart = max(utrStart, bed->thickEnd);
    }
else
    {
    utrStart = bed->chromStart + bed->chromStarts[0];
    utrEnd = utrStart + bed->blockSizes[0];
    utrEnd = min(utrEnd, bed->thickStart);
    }
return (start >= utrStart && end <= utrEnd);
}

boolean allInIntron(struct bed *bed, int start, int end)
/* Return TRUE if region lies entirely in an intron. */
{
int lastBlock = bed->blockCount - 1;
int i;
for (i=0; i<lastBlock; ++i)
    {
    int iStart = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];
    int iEnd = bed->chromStart + bed->chromStarts[i+1];
    if (start >= iStart && end <= iEnd)
        return TRUE;
    }
return FALSE;
}

int countCdsExons(struct bed *bed)
/* Count number of coding exons */
{
int count = 0;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    if (rangeIntersection(start, end, bed->thickStart, bed->thickEnd) > 0)
        ++count;
    }
return count;
}

void flagProblems(struct bed *txBed, struct slRef *goodRefList, 
	struct txInfo *info, FILE *f)
/* Write out info on problems with CDS of txBed, updating info structure 
 * writing to file. */
{
if (txBed->thickStart >= txBed->thickEnd)
    return; /* We only care about tx with CDS. */
if (refOnList(goodRefList, txBed))
    return; /* Don't flag problems in our good set. */
if (countCdsExons(txBed) > 1)
    return; /* We're mostly about retained introns. */
struct slRef *goodRef;
boolean cdsInIntron = FALSE;
boolean cdsInUtr = FALSE;
boolean cdsGoodOverlap = FALSE;
for (goodRef = goodRefList; goodRef != NULL; goodRef = goodRef->next)
    {
    struct bed *goodBed = goodRef->val;
    if (overlapInSameFrame(txBed, goodBed) >= minCdsOverlap)
        cdsGoodOverlap = TRUE;
    if (allInUtr3(goodBed, txBed->thickStart, txBed->thickEnd))
        cdsInUtr = TRUE;
    if (allInIntron(goodBed, txBed->thickStart, txBed->thickEnd))
        cdsInIntron = TRUE;
    }
if (!cdsGoodOverlap)
    {
    if (cdsInIntron)
	{
        fprintf(f, "%s\tcdsInIntron\n", txBed->name);
	info->cdsSingleInIntron = TRUE;
	}
    if (cdsInUtr)
	{
        fprintf(f, "%s\tcdsInUtr\n", txBed->name);
	info->cdsSingleInUtr3 = TRUE;
	}
    }
}

void txCdsSuspect(char *inBed, char *inTxg, char *inCluster, char *inInfo, 
	char *outSuspect, char *outInfo)
/* txCdsSuspect - Flag cases where the CDS prediction is very suspicious, including 
 * CDSs that lie entirely in an intron or in the 3' UTR of another, better looking 
 * transcript. */
{
/* Read input bed into hash.  Also calculate number with CDS set. */
struct hash *bedHash = hashNew(16);
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);
int txWithCdsCount = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->thickStart < bed->thickEnd)
        txWithCdsCount += 1;
    hashAdd(bedHash, bed->name, bed);
    }
verbose(2, "Read %d beds from %s\n", bedHash->elCount, inBed);

/* Read input transcript graphs into list, and into a hash
 * keyed by transcript names. */
struct hash *graphHash = hashNew(16);
struct txGraph *txg, *txgList = txGraphLoadAll(inTxg);
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    int i;
    for (i=0; i<txg->sourceCount; ++i)
        hashAdd(graphHash, txg->sources[i].accession, txg);
    }
verbose(2, "Read %d graphs (%d transcripts) from %s\n", slCount(txgList),
	graphHash->elCount, inTxg);

/* Read input protein cluster into list, and into a hash
 * keyed by transcript name */
struct hash *clusterHash = hashNew(16);
struct txCluster *cluster, *clusterList = txClusterLoadAll(inCluster);
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    int i;
    for (i=0; i<cluster->txCount; ++i)
        hashAdd(clusterHash, cluster->txArray[i], cluster);
    }
verbose(2, "Read %d protein clusters (%d transcripts) from  %s\n", 
	slCount(clusterList), clusterHash->elCount, inCluster);

/* Read in txInfo into a hash keyed by transcript name */
struct hash *infoHash = hashNew(16);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);
verbose(2, "Read info on %d transcripts from %s\n", infoHash->elCount, 
	inInfo);

/* Create a hash containing what looks to be the best protein-coding
 * transcript in each protein cluster.  This is keyed by cluster name
 * with a list of beds for values.  All refSeqs end up in this list.
 * If no refSeq in a cluster then the best by some heuristic ends up
 * there. */
struct hash *bestInClusterHash = hashNew(16);
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    /* Figure out best scoring one in cluster. */
    double bestScore = -BIGNUM;
    char *bestTx = NULL;
    int i;
    for (i=0; i<cluster->txCount; ++i)
        {
	char *tx = cluster->txArray[i];
	info = hashMustFindVal(infoHash, tx);
	double score = txInfoCodingScore(info, TRUE);
	if (score > bestScore)
	    {
	    bestTx = tx;
	    bestScore = score;
	    }
	}

    /* Make list of beds for cluster, starting with the best, and
     * adding in any refSeqs. */
    struct bed *bestBed = hashMustFindVal(bedHash, bestTx);
    struct bed *bedList = NULL;
    slAddHead(&bedList, bestBed);
    for (i=0; i<cluster->txCount; ++i)
        {
	char *tx = cluster->txArray[i];
	info = hashMustFindVal(infoHash, tx);
	if (info->isRefSeq && bestTx != tx)
	    {
	    struct bed *bed = hashMustFindVal(bedHash, tx);
	    slAddHead(&bedList, bed);
	    }
	}
    slReverse(&bedList);

    /* Add bed list to hash, and optionally save it to file. */
    hashAdd(bestInClusterHash, cluster->name, bedList);
    if (fNiceProt != NULL)
        {
	struct bed *bed;
	for (bed = bedList; bed != NULL; bed = bed->next)
	    fprintf(fNiceProt, "%s\t%s\n", cluster->name, bed->name);
	}
    }
verbose(2, "Picked best protein for each protein cluster\n");

/* Loop through each transcript cluster (graph).  Make a list of
 * protein clusters associated with that graph. Armed with this
 * information call flagging routine on each transcript in the graph. */
FILE *f = mustOpen(outSuspect, "w");
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    /* Build up list of protein clusters associated with transcript cluster. */
    struct slRef *protClusterRefList = NULL, *protClusterRef;
    int i;
    for (i=0; i<txg->sourceCount; ++i)
	{
	char *tx = txg->sources[i].accession;
	struct txCluster *protCluster = hashFindVal(clusterHash, tx);
	if (protCluster != NULL)
	    refAddUnique(&protClusterRefList, protCluster);
	}

    /* Figure out best scoring protein in RNA cluster, and set threshold
     * to eliminate ones scoring less than half this much. */
    double bestProtScore = 0;
    for (protClusterRef = protClusterRefList; protClusterRef != NULL;
    	protClusterRef = protClusterRef->next)
	{
	struct txCluster *protCluster = protClusterRef->val;
	struct bed *bedList = hashMustFindVal(bestInClusterHash, protCluster->name);
	struct bed *bed;
	for (bed = bedList; bed != NULL; bed = bed->next)
	    {
	    struct txInfo *info = hashMustFindVal(infoHash, bed->name);
	    double score = txInfoCodingScore(info, FALSE);
	    bestProtScore = max(score, bestProtScore);
	    }
	}
    double protScoreThreshold = bestProtScore * 0.5;

    /* Get list of references to beds of best proteins in protein clusters. */
    struct slRef *protRefList = NULL;
    for (protClusterRef = protClusterRefList; protClusterRef != NULL;
    	protClusterRef = protClusterRef->next)
	{
	struct txCluster *protCluster = protClusterRef->val;
	struct bed *bedList = hashMustFindVal(bestInClusterHash, protCluster->name);
	struct bed *bed;
	for (bed = bedList; bed != NULL; bed = bed->next)
	    {
	    struct txInfo *info = hashMustFindVal(infoHash, bed->name);
	    double score = txInfoCodingScore(info, TRUE);
	    if (score >= protScoreThreshold)
		refAdd(&protRefList, bed);
	    }
	}

    /* Go do flagging for each transcript in RNA cluster. */
    for (i=0; i<txg->sourceCount; ++i)
        {
	char *tx = txg->sources[i].accession;
	struct bed *bed = hashMustFindVal(bedHash, tx);
	struct txInfo *info = hashMustFindVal(infoHash, bed->name);
	flagProblems(bed, protRefList, info, f);
	}
    slFreeList(&protClusterRefList);
    }
carefulClose(&f);

/* Write out updated info file. */
f = mustOpen(outInfo, "w");
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
char *fileName = optionVal("niceProt", NULL);
if (fileName != NULL)
    fNiceProt = mustOpen(fileName, "w");
minAaOverlap = optionInt("minAaOverlap", minAaOverlap);
minCdsOverlap = minAaOverlap*3;
txCdsSuspect(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
carefulClose(&fNiceProt);
return 0;
}
