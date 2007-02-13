/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "geneGraph.h"
#include "txGraph.h"
#include "binRange.h"
#include "txEdgeBed.h"

boolean bedIsIntron = FALSE;
boolean bedIsHard = FALSE;
char *specificChrom = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgAddEvidence - Add evidence from a bed file to existing transcript graph.\n"
  "usage:\n"
  "   txgAddEvidence in.txg in.edges sourceType out.txg\n"
  "where in.txg is a tab-separated file in txGraph format\n"
  "      in.edges is a tab-separated file in txEdgeBed or txEdgeOrtho format\n"
  "      sourceType is a decriptive word like 'refSeq' 'est' etc.\n"
  "      out.txt is the output graph with additional info\n"
  "options:\n"
  "      -chrom=chrN - Set to only add edges for given chromosome.  Useful\n"
  "                    if in.txg is chromosome specific but in.edges is not.\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
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
/* Start search where we got last result for better performance
 * when doing things again and again. */
static int lastIx = 0;
if (lastIx < txg->sourceCount)
    {
    struct txSource *source = &txg->sources[lastIx];
    if (sameString(accession, source->accession) && sameString(type, source->type))
	return lastIx;
    }

/* Search through whole list of sources. */
int ix;
for (ix=0; ix<txg->sourceCount; ++ix)
    {
    if (ix != lastIx)
	{
	struct txSource *source = &txg->sources[ix];
	if (sameString(accession, source->accession) && sameString(type, source->type))
	    {
	    lastIx = ix;
	    return ix;
	    }
	}
    }
return -1;
}

void addEvidenceRange(struct txGraph *txg, 
	int start, enum ggVertexType startType, 
	int end,   enum ggVertexType endType,
	enum ggEdgeType edgeType, char *sourceType, char *accession)
/* Search through txg, and if can find an edge meeting the
 * given specs, add evidence to it. */
{
struct txVertex *v = txg->vertices;
struct txEdge *edge;
for (edge = txg->edges; edge != NULL; edge = edge->next)
    {
    int iStart = edge->startIx;
    int iEnd = edge->endIx;
    int eStart = v[iStart].position;
    int eEnd = v[iEnd].position;
    if (rangeIntersection(eStart, eEnd, start, end) > 0)
        {
	if (edge->type ==  edgeType)
	    {
	    boolean doIt = TRUE;
	    if (startType == ggHardStart || startType == ggHardEnd)
		{
		if (startType != v[iStart].type)
		    doIt = FALSE;
		if (eStart != start)
		    doIt = FALSE;
		}
	    if (endType == ggHardEnd || endType == ggHardStart)
		{
		if (endType != v[iEnd].type)
		    doIt = FALSE;
		if (eEnd != end)
		    doIt = FALSE;
		}
	    if (doIt)
	        {
		verbose(2, "Adding evidence at %s:%d-%d to edge of %s\n", txg->tName, start, end, txg->name);
		int sourceIx = getSourceIx(txg, sourceType, accession);
		if (sourceIx < 0)
		    {
		    sourceIx = txg->sourceCount;
		    txg->sources = ExpandArray(txg->sources, sourceIx, txg->sourceCount+1);
		    struct txSource *source = &txg->sources[sourceIx];
		    source->type = cloneString(sourceType);
		    source->accession = cloneString(accession);
		    txg->sourceCount = sourceIx + 1;
		    }
		struct txEvidence *ev;
		AllocVar(ev);
		ev->sourceId = sourceIx;
		ev->start = start;
		ev->end = end;
		slAddTail(&edge->evList, ev);
		edge->evCount += 1;
		}
	    }
	}
    }
}

void addEvidence(struct txEdgeBed *bedList, char *sourceType, struct hash *bkHash)
/* Input is a list of additional evidence in bedList, and a hash
 * full of binKeepers full of txGraphs. */
{
struct txEdgeBed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (specificChrom != NULL && !sameString(bed->chrom, specificChrom))
        continue;
    verbose(2, "Processing %s %s:%d-%d\n", bed->name, bed->chrom, bed->chromStart, bed->chromEnd);
    struct binKeeper *bk = hashMustFindVal(bkHash, bed->chrom);
    struct binElement *bel, *belList = binKeeperFind(bk, bed->chromStart, bed->chromEnd);
    verbose(3, "%s hits %d txg\n", bed->name, slCount(belList));
    for (bel = belList; bel != NULL; bel = bel->next)
        {
	struct txGraph *txg = bel->val;
	verbose(3, "  txg %s %s:%d-%d\n", txg->name, txg->tName, txg->tStart, txg->tEnd);
	if (bed->strand[0] == 0 || sameString(txg->strand, bed->strand))
	    {
	    addEvidenceRange(txg, bed->chromStart, ggVertexTypeFromString(bed->startType),
				  bed->chromEnd, ggVertexTypeFromString(bed->endType),
				  bed->type, sourceType, bed->name);
	    }
	}
    }
}

void txgAddEvidence(char *txgIn, char *bedIn, char *sourceType, char *txgOut)
/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
{
struct txGraph *txgList = txGraphLoadAll(txgIn);
verbose(2, "%d elements in txgList\n", slCount(txgList));
struct txEdgeBed *bedList = txEdgeBedLoadAll(bedIn);
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
specificChrom = optionVal("chrom", NULL);
if (argc != 5)
    usage();
txgAddEvidence(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
