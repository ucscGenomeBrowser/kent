/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "geneGraph.h"
#include "txGraph.h"
#include "binRange.h"

boolean bedIsIntron = FALSE;
boolean bedIsHard = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgAddEvidence - Add evidence from a bed file to existing transcript graph.\n"
  "usage:\n"
  "   txgAddEvidence in.txg in.bed sourceType out.txg\n"
  "where sourceType is a decriptive word like 'refSeq' 'est' etc.\n"
  "options:\n"
  "   -intron - in.bed file  is full of introns, not exons\n"
  "   -hard   - in.bed file is full of hard single exons\n"
  );
}

static struct optionSpec options[] = {
   {"intron", OPTION_BOOLEAN},
   {"hard", OPTION_BOOLEAN},
   {NULL, 0},
};

struct minChromSize
/* Associate chromosome and size. */
    {
    char *chrom;	/* Chromosome name, Not alloced here */
    int minSize;		
    };

struct hash *txgChromMinSizeHash(struct txGraph *txgList)
/* Hash full of lower bounds on chromosome sizes, taken
 * from tEnd's in txgList. */
{
struct txGraph *txg;
struct hash *sizeHash = hashNew(16);
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    struct minChromSize *chrom = hashFindVal(sizeHash, txg->tName);
    if (chrom == NULL)
        {
	lmAllocVar(sizeHash->lm, chrom);
	chrom->chrom = txg->tName;
	chrom->minSize = txg->tEnd;
	hashAdd(sizeHash, txg->tName, chrom);
	}
    else
        {
	chrom->minSize = max(chrom->minSize, txg->tEnd);
	}
    }
return sizeHash;
}

struct hash *txgIntoKeeperHash(struct txGraph *txgList)
/* Create a hash full of bin keepers (one for each chromosome or contig.
 * The binKeepers are full of txGraphs. */
{
struct hash *sizeHash = txgChromMinSizeHash(txgList);
struct hash *bkHash = hashNew(16);
struct txGraph *txg;
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    struct binKeeper *bk = hashFindVal(bkHash, txg->tName);
    if (bk == NULL)
        {
	struct minChromSize *chrom = hashMustFindVal(sizeHash, txg->tName);
	verbose(2, "New binKeeper for %s\n", txg->tName);
	bk = binKeeperNew(0, chrom->minSize);
	hashAdd(bkHash, txg->tName, bk);
	}
    binKeeperAdd(bk, txg->tStart, txg->tEnd, txg);
    }
hashFree(&sizeHash);
return bkHash;
}


int getSourceIx(struct txGraph *txg, char *type, char *accession)
/* Search support for type & accession */
{
static struct txSource *lastSource = NULL;
int lastSourceIx = -1;
if (lastSource != NULL && sameString(accession, lastSource->accession)
	&& sameString(type, lastSource->type))
    {
    return lastSourceIx;
    }
struct txSource *source;
int ix = 0;
lastSource = NULL;
for (source = txg->sources; source != NULL; source = source->next)
    {
    if (sameString(accession, source->accession) && sameString(type, source->type))
	{
        lastSource = source;
	break;
	}
    ++ix;
    }
if (lastSource)
    {
    lastSourceIx = ix;
    return ix;
    }
else
    return -1;
}

void addEvidenceRange(struct txGraph *txg, 
	int start, enum ggVertexType startType, 
	int end,   enum ggVertexType endType,
	enum ggEdgeType edgeType, char *sourceType, char *accession)
/* Search through txg, and if can find an edge meeting the
 * given specs, add evidence to it. */
{
int i;
int *vPositions = txg->vPositions;
for (i=0; i<txg->edgeCount; ++i)
    {
    int iStart = txg->edgeStarts[i];
    int iEnd = txg->edgeEnds[i];
    int eStart = vPositions[iStart];
    int eEnd = vPositions[iEnd];
    if (rangeIntersection(eStart, eEnd, start, end) > 0)
        {
	if (txg->edgeTypes[i] ==  edgeType)
	    {
	    boolean doIt = TRUE;
	    if (startType == ggHardStart || startType == ggHardEnd)
		{
		if (startType != txg->vTypes[iStart])
		    doIt = FALSE;
		if (eStart != start)
		    doIt = FALSE;
		}
	    if (endType == ggHardEnd || endType == ggHardStart)
		{
		if (endType != txg->vTypes[iEnd])
		    doIt = FALSE;
		if (eEnd != end)
		    doIt = FALSE;
		}
	    if (doIt)
	        {
		uglyf("Adding evidence at %s:%d-%d to edge %d of %s\n", txg->tName, start, end, i, txg->name);
		uglyf("evCount = %d\n", txg->evidence->evCount);
		uglyf("slCount(evList) = %d\n", slCount(txg->evidence->evList));
		int sourceIx = getSourceIx(txg, sourceType, accession);
		if (sourceIx < 0)
		    {
		    struct txSource *source;
		    AllocVar(source);
		    source->type = cloneString(sourceType);
		    source->accession = cloneString(accession);
		    slAddTail(&txg->sources, source);
		    sourceIx = txg->sourceCount;
		    txg->sourceCount += 1;
		    }
		struct txEvidence *ev;
		AllocVar(ev);
		ev->sourceId = sourceIx;
		ev->start = start;
		ev->end = end;
		slAddTail(&txg->evidence->evList, ev);
		txg->evidence->evCount += 1;
		}
	    }
	}
    }
}

void addBedEvidence(struct bed *bed, char *sourceType, struct txGraph *txg)
/* Add evidence from bed file to txg */
{
if (bedIsIntron)
    {
    addEvidenceRange(txg, bed->chromStart, ggHardEnd, bed->chromEnd, ggHardStart, 
        ggIntron, sourceType, bed->name);
    }
else if (bedIsHard)
    {
    addEvidenceRange(txg, bed->chromStart, ggHardStart, bed->chromEnd, ggHardEnd, 
        ggExon, sourceType, bed->name);
    }
else
    {
    int blockCount = bed->blockCount;
    if (blockCount <= 1)
	addEvidenceRange(txg, bed->chromStart, ggSoftStart, bed->chromEnd, ggSoftEnd, 
	    ggExon, sourceType, bed->name);
    else
        {
	/* Add first exon. */
	int bedStart = bed->chromStart;
	int start = bed->chromStarts[0] + bedStart;
	int end = start + bed->blockSizes[0];
	addEvidenceRange(txg, start, ggSoftStart, end, ggHardEnd, 
		ggExon, sourceType, bed->name);

	/* Add remaining exons. */
	int i;
	for (i=1; i<blockCount; ++i)
	    {
	    /* Add preceding intron. */
	    start = bed->chromStarts[i] + bedStart;
	    addEvidenceRange(txg, end, ggHardEnd, start, ggHardStart, 
	    	ggIntron, sourceType, bed->name);

	    /* Add current exon */
	    end = start + bed->blockSizes[i];
	    boolean isLast = (i == blockCount-1);
	    addEvidenceRange(txg, start, ggHardStart, end, 
	    	(isLast ? ggSoftEnd : ggHardEnd),
		ggExon, sourceType, bed->name);
	    }
	}
    }
}

void addEvidence(struct bed *bedList, char *sourceType, struct hash *bkHash)
/* Input is a list of additional evidence in bedList, and a hash
 * full of binKeepers full of txGraphs. */
{
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    verbose(2, "Processing %s %s:%d-%d\n", bed->name, bed->chrom, bed->chromStart, bed->chromEnd);
    struct binKeeper *bk = hashMustFindVal(bkHash, bedList->chrom);
    struct binElement *bel, *belList = binKeeperFind(bk, bed->chromStart, bed->chromEnd);
    verbose(3, "%s hits %d txg\n", bed->name, slCount(belList));
    for (bel = belList; bel != NULL; bel = bel->next)
        {
	struct txGraph *txg = bel->val;
	verbose(3, "  txg %s:%d-%d\n", txg->tName, txg->tStart, txg->tEnd);
	if (bed->strand[0] == 0 || sameString(txg->strand, bed->strand))
	    {
	    addBedEvidence(bed, sourceType, txg);
	    }
	}
    }
}

void txgAddEvidence(char *txgIn, char *bedIn, char *sourceType, char *txgOut)
/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
{
struct txGraph *txgList = txGraphLoadAll(txgIn);
verbose(2, "%d elements in txgList\n", slCount(txgList));
struct bed *bedList = bedLoadAll(bedIn);
verbose(2, "%d elements in bedList\n", slCount(bedList));
FILE *f = mustOpen(txgOut, "w");

verbose(1, "Adding %s evidence from %s to %s. Result is in %s.\n",
	sourceType, bedIn, txgIn, txgOut);
struct hash *bkHash = txgIntoKeeperHash(txgList);
verbose(2, "Loaded into keeper hash of %d elements\n", bkHash->elCount);

addEvidence(bedList, sourceType, bkHash);
verbose(2, "Added evidence.\n");

struct txGraph *txg;
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    txGraphTabOut(txg, f);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
bedIsIntron = optionExists("intron");
bedIsHard = optionExists("hard");
if (argc != 5)
    usage();
txgAddEvidence(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
