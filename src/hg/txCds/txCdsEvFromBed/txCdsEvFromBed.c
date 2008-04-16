/* txCdsEvFromBed - Make a cds evidence file (.tce) from an existing bed file.  
 * Used mostly in transferring CCDS coding regions currently.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dystring.h"
#include "binRange.h"
#include "bed.h"
#include "txRnaAccs.h"
#include "twoBit.h"


static char const rcsid[] = "$Id: txCdsEvFromBed.c,v 1.3 2008/04/16 15:24:27 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsEvFromBed - Make a cds evidence file (.tce) from an existing bed file.  Used mostly \n"
  "in transferring CCDS coding regions currently.\n"
  "usage:\n"
  "   txCdsEvFromBed cds.bed type tx.bed genome.2bit output.tce\n"
  "where:\n"
  "   cds.bed is a bed12 format file with thickStart/thickEnd defining coding region\n"
  "   type is the name that will appear in the type column of output.tce\n"
  "   tx.bed is a bed12 format file containing the transcripts we're annotating\n"
  "   genome.2bit is a file containing DNA sequence for the associated genome\n"
  "   output.tce is the tab-delimited output in the same format used by txCdsEvFromRna\n"
  "      txCdsEvFromBed, txCdsPick and txCdsToGene.  It mostly defines the CDS in terms of\n"
  "      the virtual transcript\n"
  "example:\n"
  "    txCdsEvFromBed ccds.bed ccds txWalk.bed hg18.2bit ccds.tce\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct cdsTxPair
/* A pair of beds - one a coding region, the other an enclosing transcript. */
    {
    struct cdsTxPair *next;	/* Next in list. */
    struct bed *cds;		/* Coding region. */
    struct bed *tx;		/* Transcript. */
    struct usedBed *usedCds;	/* CDS with a flag. */
    struct usedBed *usedTx;	/* Transcript with a flag. */
    };

struct usedBed
/* Keeps track of whether a bed is used */
    {
    struct usedBed *next;
    struct bed *bed;	/* Pointer to underlying bed. */
    struct slRef *pairList;	/* List of pairs using this bed. */
    boolean used;	/* True if BED is used. */
    };

struct cdsTxCluster
/* A cluster of pairs - a pairs that share a tx or a cds. */
    {
    struct cdsTxCluster *next;	/* Next in list. */
    struct usedBed *txList;     /* List of transcripts used. */
    struct usedBed *cdsList;	/* List of cds's used. */
    struct slRef *pairList;	/* List of pairs. */
    };

int usedBedCmpRnaSize(const void *va, const void *vb)
/* Compare to sort based on largest total rna length with largest first. */
{
const struct usedBed *a = *((struct usedBed **)va);
const struct usedBed *b = *((struct usedBed **)vb);
return bedTotalBlockSize(b->bed) - bedTotalBlockSize(a->bed);
}

void outputCdsEv(struct bed *tx, struct bed *cds, char *tceType, FILE *f)
/* Output CDS for transcript. */
{
int txSize = bedTotalBlockSize(tx);
int cdsStartInRna = bedBlockSizeInRange(tx, tx->chromStart, cds->chromStart);
int cdsEndInRna = bedBlockSizeInRange(tx, tx->chromStart, cds->chromEnd);
if (tx->strand[0] == '-')
    reverseIntRange(&cdsStartInRna, &cdsEndInRna, txSize);
fprintf(f, "%s\t", tx->name);
fprintf(f, "%d\t", cdsStartInRna);
fprintf(f, "%d\t", cdsEndInRna);
fprintf(f, "%s\t", tceType);
fprintf(f, "%s\t", cds->name);
fprintf(f, "1000\t");	// score
fprintf(f, "1\t");	// starts with start codon
fprintf(f, "1\t");	// ends with stop codon
fprintf(f, "1\t");	// block count
fprintf(f, "%d,\t", cdsStartInRna);
fprintf(f, "%d,\n", cdsEndInRna - cdsStartInRna);
}

struct dyString *dnaOfIntersection(struct bed *bed, int rangeStart, int rangeEnd, 
	struct twoBitFile *tbf)
/* Fetch DNA of part of (blocked) bed that is between rangeStart/rangeEnd. */
{
int i;
struct dyString *dy = dyStringNew(0);
for (i=0; i<bed->blockCount; ++i)
    {
    int blockStart = bed->chromStart + bed->chromStarts[i];
    int blockEnd = blockStart + bed->blockSizes[i];
    if (blockStart < rangeStart) blockStart = rangeStart;
    if (blockEnd > rangeEnd) blockEnd = rangeEnd;
    if (blockStart < blockEnd)
        {
	struct dnaSeq *seq = twoBitReadSeqFragLower(tbf, bed->chrom, blockStart, blockEnd);
	dyStringAppendN(dy, seq->dna, seq->size);
	dnaSeqFree(&seq);
	}
    }
return dy;
}

boolean upstreamStartInString(struct dyString *utr5)
/* Give 5' UTR sequence return TRUE if there is a start codon in frame before any
 * start codons, starting from end. */
{
int codonStart;
for (codonStart = utr5->stringSize - 3; codonStart >= 0; codonStart -= 3)
    {
    DNA *codon = utr5->string + codonStart;
    if (startsWith("atg", codon))
        return TRUE;
    if (isStopCodon(codon))
        return FALSE;
    }
return FALSE;
}

boolean upstreamStartToCds(struct bed *cds, struct bed *tx, struct twoBitFile *tbf)
/* Return TRUE if there is a start codon in tx that precedes cds. This does allow
 * upstream ORFs though - where there is a stop codon and then a start codon. */
{
struct dyString *dna = NULL;
if (tx->strand[0] == '-')
   {
   dna = dnaOfIntersection(tx, cds->chromEnd, tx->chromEnd, tbf);
   reverseComplement(dna->string, dna->stringSize);
   }
else
   {
   dna = dnaOfIntersection(tx, tx->chromStart, cds->chromStart, tbf);
   }
boolean gotStart = upstreamStartInString(dna);
dyStringFree(&dna);
return gotStart;
}

int scoreCdsMatch(struct bed *cds, struct bed *tx, struct twoBitFile *tbf)
/* Score based on how well tx handles cds as a subset.  This is an ad-hoc score with
 * the following properties:
 *    Large minus score for cds not being a proper subset of tx
 *    Moderate minus score for having an upstream start codon
 *    Slight minus score for having bases in UTR. */
{
if (!bedCompatibleExtension(cds, tx))
    return -1000000;
if (upstreamStartToCds(cds, tx, tbf))
    return -100000;
return bedTotalBlockSize(cds) - bedTotalBlockSize(tx);
}

struct usedBed *findBestTxForCds(struct usedBed *cdsUsed, struct usedBed *txList,
    struct twoBitFile *tbf, boolean useTarget)
/* Return transcript on list that is best match to cds.  This uses score 
 * function defined above. */
{
struct bed *cds = cdsUsed->bed;
struct usedBed *bestTx = NULL;
int bestScore = -BIGNUM;
struct usedBed *txUsed;
for (txUsed = txList; txUsed != NULL; txUsed = txUsed->next)
    {
    if (txUsed->used == useTarget)
        {
	int score = scoreCdsMatch(cds, txUsed->bed, tbf);
	if (score > bestScore)
	    {
	    bestScore = score;
	    bestTx = txUsed;
	    }
	}
    }
return bestTx;
}

void txCdsEvOnCluster(struct cdsTxCluster *cluster, struct twoBitFile *tbf, char *tceType, FILE *f)
/* Given a cluster of transcripts and cds's that are compatible, try to make sure
 * there is one transcript for each cds, and that each cds has a unique transcript,
 * and that where possible there are no upstream start codons for a CDS. */
{
verbose(3, "Cluster %d %d %d:\n", slCount(cluster->pairList), 
	slCount(cluster->txList), slCount(cluster->cdsList));
if (cluster->txList->next == NULL && cluster->cdsList->next == NULL)
    {
    /* Special case for clusters with only one CDS and one tx. */
    outputCdsEv(cluster->txList->bed, cluster->cdsList->bed, tceType, f);
    }
else
    {
    /* Clear used flags for txList. */
    struct usedBed *tx;
    for (tx = cluster->txList; tx != NULL; tx = tx->next)
	tx->used = FALSE;

    /* Sort cdsList so that the biggest one will be first, and then find best (unused)
     * transcript for each cds. */
    slSort(&cluster->cdsList, usedBedCmpRnaSize);
    struct usedBed *cds;
    for (cds = cluster->cdsList;  cds != NULL; cds = cds->next)
	{
	tx = findBestTxForCds(cds, cluster->txList, tbf, FALSE);
	if (tx == NULL)
	    {
	    tx = findBestTxForCds(cds, cluster->txList, tbf, TRUE);
	    assert(tx != NULL);
	    warn("Couldn't find unused transcript for %s, reusing %s",
		    cds->bed->name, tx->bed->name);
	    }
	tx->used = TRUE;
	outputCdsEv(tx->bed, cds->bed, tceType, f);
	}
    }
}

struct usedBed *addToUsedBedHash(struct hash *hash, struct bed *bed, struct cdsTxPair *pair)
/* If bed name is new, create a usedBed for it and add it to hash and list. */
{
struct hashEl *hel;
if ((hel = hashLookup(hash, bed->name)) == NULL)
    {
    struct usedBed *ub;
    AllocVar(ub);
    ub->bed = bed;
    hel = hashAdd(hash, bed->name, ub);
    }
struct usedBed *ub = hel->val;
refAdd(&ub->pairList, pair);
return ub;
}

void rAddPair(struct cdsTxCluster *cluster, struct cdsTxPair *pair)
/* Add pair to cluster.   Also add any pairs connected to this pair to cluster. */
{
struct usedBed *cds = pair->usedCds, *tx = pair->usedTx;
if (cds->used && tx->used)
    return;
refAdd(&cluster->pairList, pair);
if (!cds->used)
    {
    slAddHead(&cluster->cdsList, cds);
    cds->used = TRUE;
    struct slRef *ref;
    for (ref = cds->pairList; ref != NULL; ref = ref->next)
        rAddPair(cluster, ref->val);
    }
if (!tx->used)
    {
    slAddHead(&cluster->txList, tx);
    tx->used = TRUE;
    struct slRef *ref;
    for (ref = tx->pairList; ref != NULL; ref = ref->next)
        rAddPair(cluster, ref->val);
    }
}


void txCdsEvOnChrom(struct cdsTxPair *pairList, struct twoBitFile *tbf, char *tceType, FILE *f)
/* Cluster together pairs that share any common elements, and then call routine
 * to further process each cluster. */
{
/* Create hashes of uniq beds with a usage flag. */
struct cdsTxPair *pair;
struct hash *txHash = hashNew(18), *cdsHash = hashNew(18);
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    pair->usedTx = addToUsedBedHash(txHash, pair->tx, pair);
    pair->usedCds = addToUsedBedHash(cdsHash, pair->cds, pair);
    }
verbose(2, "Chrom %s has %d tx and %d cds\n", pairList->tx->chrom, txHash->elCount, cdsHash->elCount);

/* Construct clusters, somewhat recursively. */
struct cdsTxCluster *cluster, *clusterList = NULL;
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (!pair->usedTx->used || !pair->usedCds->used)
        {
	AllocVar(cluster);
	slAddHead(&clusterList, cluster);
	rAddPair(cluster, pair);
	}
    }
slReverse(&clusterList);

/* Do further processing on each cluster. */
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    txCdsEvOnCluster(cluster, tbf, tceType, f);
    }
}


struct cdsTxPair *getCompatiblePairs(struct bed *txBedList, struct bed *cdsBedList)
/* Given list of transcripts and cds's, get list of all compatible tx/cds pairs. */
{
struct cdsTxPair *pairList = NULL;
struct bed *cdsBed;
struct hash *txKeepers = bedsIntoKeeperHash(txBedList);

for (cdsBed = cdsBedList; cdsBed != NULL; cdsBed = cdsBed->next)
    {
    struct cdsTxPair *pair = NULL;
    struct binKeeper *bk = hashFindVal(txKeepers, cdsBed->chrom);
    if (bk != NULL)
        {
	struct binElement *bel, *belList = binKeeperFind(bk, cdsBed->chromStart, 
		cdsBed->chromEnd);
	for (bel = belList; bel != NULL; bel = bel->next)
	    {
	    struct bed *txBed = bel->val;
	    if (bedCompatibleExtension(cdsBed, txBed))
	        {
		AllocVar(pair);
		pair->cds = cdsBed;
		pair->tx = txBed;
		slAddHead(&pairList, pair);
		}
	    }
	slFreeList(&belList);
	}
    if (pair == NULL)
        warn("No tx for %s\n", cdsBed->name);
    }
hashFreeWithVals(&txKeepers, binKeeperFree);
slReverse(&pairList);
return pairList;
}


void txCdsEvFromBed(char *cdsBedFile, char *tceType, char *txBedFile, char *genomeSeq,
	char *outFile)
/* txCdsEvFromBed - Make a cds evidence file (.tce) from an existing bed file.  Used 
 * mostly in transferring CCDS coding regions currently. */
{
/* Load cds and tx beds, genome, and get list of all compatible pairs. */
struct bed *txBedList = bedLoadNAll(txBedFile, 12);
struct bed *cdsBedList = bedLoadNAll(cdsBedFile, 12);
struct cdsTxPair *pairList = getCompatiblePairs(txBedList, cdsBedList);

/* Separate pairs into chromosomes.  This is mostly to avoid problems with ccds IDs 
 * only being unique per-chromosome.  (The pseudo-autosomal regions of X and Y have
 * genes with the same CCDS id. */
struct hash *chromHash = hashNew(0);
struct cdsTxPair *pair, *nextPair;
for (pair = pairList; pair != NULL; pair = nextPair)
    {
    nextPair = pair->next;
    struct hashEl *hel = hashLookup(chromHash, pair->cds->chrom);
    if (hel == NULL)
        hel = hashAdd(chromHash, pair->cds->chrom, NULL);
    slAddHead(&hel->val, pair);
    }

/* Open output file, DNA file, and call routine to process each chromosome. */
FILE *f = mustOpen(outFile, "w");
struct twoBitFile *tbf = twoBitOpen(genomeSeq);
struct hashEl *hel, *helList = hashElListHash(chromHash);
for (hel = helList; hel != NULL; hel = hel->next)
     txCdsEvOnChrom(hel->val, tbf, tceType, f);

/* Clean up and go home. */
twoBitClose(&tbf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dnaUtilOpen();
if (argc != 6)
    usage();
txCdsEvFromBed(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
