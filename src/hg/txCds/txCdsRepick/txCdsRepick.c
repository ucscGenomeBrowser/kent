/* txCdsRepick - After we have clustered based on the preliminary coding 
 * regions we can make a more intelligent choice here about the final coding 
 * regions. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txInfo.h"
#include "txCluster.h"
#include "txGraph.h"
#include "cdsEvidence.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsRepick - After we have clustered based on the preliminary coding\n"
  "regions we can make a more intelligent choice here about the final coding\n"
  "regions.\n"
  "usage:\n"
  "   txCdsRepick input.bed input.txg input.cluster input.info input.tce output.tce\n"
  "where:\n"
  "   input.bed contains the genes with preliminary CDS annotations\n"
  "   input.txg is a txBedToGraph result on the same genes that holds\n"
  "             the RNA-level clustering information\n"
  "   input.cluster is a txCdsCluster result on the same genes that\n"
  "             holds the protein-level clustering information\n"
  "   input.info is a txInfoAssemble result that contains various useful\n"
  "             bits of information on the gene\n"
  "   input.tce is a txCdsPick result (weighted.tce) containing ranked\n"
  "             CDS choices.\n"
  "   output.tce contains one pick for each coding gene\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

double infoCodingScore(struct txInfo *info, boolean boostRefSeq)
/* Return coding score for info.  This is just the bestorf score,
 * plus another 750 if it's a refSeq.  750 is quite a bit for a
 * bestorf score, only about 1% of proteins score more than that. 
 * If it's an NMD target then divide by 50. */
{
double score = info->bestorfScore;
if (boostRefSeq && info->isRefSeq)
    score += 750;
if (info->nonsenseMediatedDecay)
    score *= 0.02;
return score;
}

void pickCompatableCds(struct bed *bed, struct slRef *protRefList, 
	struct cdsEvidence *evList, FILE *f)
/* Given a bed, a list of protein-coding beds to be compatible with,
 * and a sorted list of possible CDS's, write first CDS if any that
 * is compatible with any on list to file. */
{
}

void txCdsRepick(char *inputBed, char *inputTxg, char *inputCluster, 
	char *inputInfo, char *inputCds, char *outputCds)
/* txCdsRepick - After we have clustered based on the preliminary coding 
 * regions we can make a more intelligent choice here about the final coding 
 * regions. */
{
/* Read input bed into hash */
struct hash *bedHash = hashNew(16);
struct bed *bed, *bedList = bedLoadNAll(inputBed, 12);
for (bed = bedList; bed != NULL; bed = bed->next)
    hashAdd(bedHash, bed->name, bed);
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
	hashAdd(bestInClusterHash, cluster->name, bestTx);
	}
    }
verbose(2, "Picked best protein for each protein cluster\n");

FILE *f = mustOpen(outputCds, "w");

/* Loop through each transcript cluster (graph).  Make a list of
 * protein clusters associated with that graph. Armed with this
 * information call repick routine on each transcript in the graph. */
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
	pickCompatableCds(bed, protRefList, evList, f);
	}
    slFreeList(&protClusterRefList);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
txCdsRepick(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
