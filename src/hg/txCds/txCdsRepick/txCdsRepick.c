/* txCdsRepick - OBSOLETE program. The scheme this implemented ended up
 * not working so well. It's still in the source tree because it may contain
 * some useful routines for other programs */
/* txCdsRepick - After we have clustered based on the preliminary coding 
 * regions we can make a more intelligent choice here about the final coding 
 * regions. */

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

int minAaOverlap = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsRepick - OBSOLETE program. The scheme this implemented ended up\n"
  "not working so well. It's still in the source tree because it may contain\n"
  "some useful routines for other programs\n"
  "Here's the original usage statement:\n"
  "\n"
  "After we have clustered based on the preliminary coding\n"
  "regions we can make a more intelligent choice here about the final coding\n"
  "regions.\n"
  "usage:\n"
  "   txCdsRepick in.bed in.txg in.cluster in.info in.tce out.tce out.pp\n"
  "where:\n"
  "   in.bed contains the genes with preliminary CDS annotations\n"
  "   in.txg is a txBedToGraph result on the same genes that holds\n"
  "             the RNA-level clustering information\n"
  "   in.cluster is a txCdsCluster result on the same genes that\n"
  "             holds the protein-level clustering information\n"
  "   in.info is a txInfoAssemble result that contains various useful\n"
  "             bits of information on the gene\n"
  "   in.tce is a txCdsPick result (weighted.tce) containing ranked\n"
  "             CDS choices.\n"
  "   out.tce contains one pick for each coding gene\n"
  "   out.pp is a two column file. First column is protein-cluster ID\n"
  "               Second column is \"best\" coding transcript in cluster.\n"
  "options:\n"
  "   -minAaOverlap=N - minimum number of amino acids to overlap with to consider\n"
  "                     a CDS pick compatible with one of the primary proteins for\n"
  "                     the cluster\n"
  );
}

static struct optionSpec options[] = {
   {"minAaOverlap", OPTION_INT},
   {NULL, 0},
};

double infoCodingScore(struct txInfo *info, boolean boostRefSeq)
/* Return coding score for info.  This is just the bestorf score,
 * plus another 750 if it's a refSeq.  750 is quite a bit for a
 * bestorf score, only about 1% of proteins score more than that. 
 * If it's an NMD target then divide by 50. */
{
double score = info->cdsScore;
if (boostRefSeq && info->isRefSeq)
    score += 750;
if (info->nonsenseMediatedDecay)
    score *= 0.02;
return score;
}

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

boolean bedCompatibleWithList(struct bed *bed, struct slRef *protRefList)
/* Return TRUE if bed is compatible (overlaps in frame enough) with
 * one of the items on the protRefList. */
{
struct slRef *protRef;
for (protRef = protRefList; protRef != NULL; protRef = protRef->next)
    {
    struct bed *protBed = protRef->val;
    if (overlapInSameFrame(bed, protBed) >= minAaOverlap)
        return TRUE;
    }
return FALSE;
}

/* Keep track of how many CDS picks we alter. */
int pickedBetter = 0, pickedNone = 0;

void pickCompatableCds(struct bed *bed, struct slRef *protRefList, 
	struct cdsEvidence *evList, struct txInfo *info, FILE *f)
/* Given a bed, a list of protein-coding beds to be compatible with,
 * and a sorted list of possible CDS's, write first CDS if any that
 * is compatible with any on list to file. */
{
if (info->isRefSeq || bedCompatibleWithList(bed, protRefList))
    {
    cdsEvidenceTabOut(evList, f);
    }
else
    {
    struct cdsEvidence *ev;
    for (ev = evList->next; ev != NULL; ev = ev->next)
	{
	if (ev->cdsCount == 1)
	    {
	    cdsEvidenceSetBedThick(ev, bed, TRUE);
	    if (bedCompatibleWithList(bed, protRefList))
		{
		cdsEvidenceTabOut(ev, f);
		verbose(3, "Repicking CDS for %s, new one is based on %s %s score %f\n", 
			bed->name, ev->source, ev->accession, ev->score);
		++pickedBetter;
		break;
		}
	    }
	}
    if (ev == NULL)
	{
	verbose(3, "Removing CDS from %s\n", bed->name);
        ++pickedNone;
	}
    }
}

void txCdsRepick(char *inputBed, char *inputTxg, char *inputCluster, 
	char *inputInfo, char *inputCds, char *outputCds, char *outputPp)
/* txCdsRepick - After we have clustered based on the preliminary coding 
 * regions we can make a more intelligent choice here about the final coding 
 * regions. */
{
/* Read input bed into hash.  Also calculate number with CDS set. */
struct hash *bedHash = hashNew(16);
struct bed *bed, *bedList = bedLoadNAll(inputBed, 12);
int txWithCdsCount = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->thickStart < bed->thickEnd)
        txWithCdsCount += 1;
    hashAdd(bedHash, bed->name, bed);
    }
verbose(2, "Read %d beds from %s\n", bedHash->elCount, inputBed);

/* Read input transcript graphs into list, and into a hash
 * keyed by transcript names. */
struct hash *graphHash = hashNew(16);
struct txGraph *txg, *txgList = txGraphLoadAll(inputTxg);
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    int i;
    for (i=0; i<txg->sourceCount; ++i)
        hashAdd(graphHash, txg->sources[i].accession, txg);
    }
verbose(2, "Read %d graphs (%d transcripts) from %s\n", slCount(txgList),
	graphHash->elCount, inputTxg);

/* Read input protein cluster into list, and into a hash
 * keyed by transcript name */
struct hash *clusterHash = hashNew(16);
struct txCluster *cluster, *clusterList = txClusterLoadAll(inputCluster);
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    int i;
    for (i=0; i<cluster->txCount; ++i)
        hashAdd(clusterHash, cluster->txArray[i], cluster);
    }
verbose(2, "Read %d protein clusters (%d transcripts) from  %s\n", 
	slCount(clusterList), clusterHash->elCount, inputCluster);

/* Read in txInfo into a hash keyed by transcript name */
struct hash *infoHash = hashNew(16);
struct txInfo *info, *infoList = txInfoLoadAll(inputInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);
verbose(2, "Read info on %d transcripts from %s\n", infoHash->elCount, 
	inputInfo);

/* Read in input cds evidence into a hash keyed by transcript name
 * who's values are a sorted *list* of evidence. */
struct hash *evHash = hashNew(16);
struct cdsEvidence *ev, *nextEv, *evList = cdsEvidenceLoadAll(inputCds);
int evCount = 0;
for  (ev = evList; ev != NULL; ev = nextEv)
    {
    nextEv = ev->next;
    struct hashEl *hel = hashLookup(evHash, ev->name);
    if (hel == NULL)
        hel = hashAdd(evHash, ev->name, NULL);
    slAddTail(&hel->val, ev);
    ++evCount;
    }
verbose(2, "Read %d pieces of cdsEvidence on %d transcripts from %s\n",
	evCount, evHash->elCount, inputCds);

/* Create a hash containing what looks to be the best protein-coding
 * transcript in each protein cluster.  This is keyed by cluster name
 * with transcript names for values. */
FILE *f = mustOpen(outputPp, "w");
struct hash *bestInClusterHash = hashNew(16);
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    double bestScore = -BIGNUM;
    char *bestTx = NULL;
    int i;
    for (i=0; i<cluster->txCount; ++i)
        {
	char *tx = cluster->txArray[i];
	info = hashMustFindVal(infoHash, tx);
	double score = infoCodingScore(info, TRUE);
	if (score > bestScore)
	    {
	    bestTx = tx;
	    bestScore = score;
	    }
	}
    hashAdd(bestInClusterHash, cluster->name, bestTx);
    fprintf(f, "%s\t%s\n", cluster->name, bestTx);
    }
carefulClose(&f);
verbose(2, "Picked best protein for each protein cluster\n");


/* Loop through each transcript cluster (graph).  Make a list of
 * protein clusters associated with that graph. Armed with this
 * information call repick routine on each transcript in the graph. */
f = mustOpen(outputCds, "w");
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
	char *protTx = hashMustFindVal(bestInClusterHash, protCluster->name);
	struct txInfo *info = hashMustFindVal(infoHash, protTx);
	double score = infoCodingScore(info, FALSE);
	bestProtScore = max(score, bestProtScore);
	}
    double protScoreThreshold = bestProtScore * 0.5;

    /* Get list of references to beds of proteins over that threshold. */
    struct slRef *protRefList = NULL;
    for (protClusterRef = protClusterRefList; protClusterRef != NULL;
    	protClusterRef = protClusterRef->next)
	{
	struct txCluster *protCluster = protClusterRef->val;
	char *protTx = hashMustFindVal(bestInClusterHash, protCluster->name);
	struct txInfo *info = hashMustFindVal(infoHash, protTx);
	double score = infoCodingScore(info, FALSE);
	if (score >= protScoreThreshold)
	    {
	    struct bed *bed = hashMustFindVal(bedHash, protTx);
	    refAdd(&protRefList, bed);
	    }
	}

    /* Go repick each CDS in RNA cluster */
    for (i=0; i<txg->sourceCount; ++i)
        {
	char *tx = txg->sources[i].accession;
	struct bed *bed = hashMustFindVal(bedHash, tx);
	struct cdsEvidence *evList = hashFindVal(evHash, tx);
	if (evList != NULL && bed->thickStart < bed->thickEnd)
	    {
	    info = hashMustFindVal(infoHash, bed->name);
	    pickCompatableCds(bed, protRefList, evList, info, f);
	    }
	}
    slFreeList(&protClusterRefList);
    }
carefulClose(&f);
verbose(1, "repicked %d, removed %d, no change to %d\n",
    pickedBetter, pickedNone, txWithCdsCount - pickedBetter - pickedNone);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 8)
    usage();
minAaOverlap = optionInt("minAaOverlap", minAaOverlap);
txCdsRepick(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
