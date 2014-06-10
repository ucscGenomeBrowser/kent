/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "geneGraph.h"
#include "txGraph.h"
#include "ggTypes.h"
#include "binRange.h"
#include "txEdgeBed.h"

int maxBleedOver = 6;
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
  "      -maxBleedOver=N - Maximum amount of exon that can be lost when snapping\n"
  "                    soft to hard edges. Default %d\n"
  , maxBleedOver
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"maxBleedOver", OPTION_STRING},
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

boolean evIsHard(char *type)
/* Return TRUE if txEdgeBed start/end type is hard. */
{
char c = type[0];
return c == '[' || c == ']';
}

boolean vertexIsHard(struct txVertex *vertex)
/* Return true if a hard start or hard end. */
{
unsigned char c = vertex->type;
return c == ggHardStart || c == ggHardEnd;
}

int evEdgeScore(struct txGraph *txg, struct txEdge *edge, 
	struct txEdgeBed *ev)
/* Return a score for edge/evidence match up. There's two main
 * factors in the score: the extent of the overlap, and the
 * agreement of the hardness/softness of the ends. */
{
/* Check that introns go with introns and exons with exons. */
if ((int)edge->type != (int)ev->type)
    {
    return 0;
    }

/* Unpack some of the input data structures into local variables
 * and see if we even overlap. */
struct txVertex *startVertex = &txg->vertices[edge->startIx];
struct txVertex *endVertex = &txg->vertices[edge->endIx];
int edgeStart = startVertex->position;
int edgeEnd = endVertex->position;
int evStart = ev->chromStart;
int evEnd = ev->chromEnd;
int overlapSize = rangeIntersection(edgeStart, edgeEnd, evStart, evEnd);
if (overlapSize <= 0)
    {
    return 0;
    }

/* Figure out hardness/softness of starts and ends of both evidence
 * and edge. */
boolean edgeHardStart = vertexIsHard(startVertex);
boolean edgeHardEnd = vertexIsHard(endVertex);
boolean evHardStart = evIsHard(ev->startType);
boolean evHardEnd = evIsHard(ev->endType);

/* Go through 4 possible cases at start */
int startBonus = 0;
if (evHardStart && edgeHardStart)
    {
    if (evStart != edgeStart)
        return 0;
    startBonus = 2000000;
    }
else if (evHardStart && !edgeHardStart)
    {
    if (evStart - edgeStart > maxBleedOver)
        return 0;
    }
else if (!evHardStart && edgeHardStart)
    {
    if (edgeStart - evStart > maxBleedOver)
        return 0;
    }
else if (!evHardStart && !edgeHardStart)
    {
    /* Both soft, no real constraints */
    }

/* Go through 4 possible cases at end */
int endBonus = 0;
if (evHardEnd && edgeHardEnd)
    {
    if (evEnd != edgeEnd)
        return 0;
    endBonus = 2000000;
    }
else if (evHardEnd && !edgeHardEnd)
    {
    if (edgeEnd - evEnd > maxBleedOver)
        return 0;
    }
else if (!evHardEnd && edgeHardEnd)
    {
    if (evEnd - edgeEnd > maxBleedOver)
        return 0;
    }
else if (!evHardEnd && !edgeHardEnd)
    {
    /* Both soft, no real constraints */
    }

return startBonus + endBonus + overlapSize;
}

void addEvidenceToGraph(struct txGraph *txg, struct txEdgeBed *ev, 
	char *sourceType)
/* Search through txg, and if can find an edge that agrees with 
 * ev, add ev to best agreeing edge. */
{
struct txEdge *edge, *bestEdge = NULL;
int score, bestScore = 0;
for (edge = txg->edgeList; edge != NULL; edge = edge->next)
    {
    score = evEdgeScore(txg, edge, ev);
    if (score > bestScore)
        {
	bestScore = score;
	bestEdge = edge;
	}
    }
if (bestEdge != NULL)
    {
    verbose(2, "Adding evidence at %s:%d-%d to edge of %s\n", 
	    txg->tName, ev->chromStart, ev->chromEnd, txg->name);
    char *accession = ev->name;
    int sourceIx = getSourceIx(txg, sourceType, accession);
    if (sourceIx < 0)
	{
	sourceIx = txg->sourceCount;
	ExpandArray(txg->sources, sourceIx, txg->sourceCount+1);
	struct txSource *source = &txg->sources[sourceIx];
	source->type = cloneString(sourceType);
	source->accession = cloneString(accession);
	txg->sourceCount = sourceIx + 1;
	}
    struct txEvidence *tev;
    AllocVar(tev);
    tev->sourceId = sourceIx;
    tev->start = ev->chromStart;
    tev->end = ev->chromEnd;
    slAddTail(&bestEdge->evList, tev);
    bestEdge->evCount += 1;
    }
}

void addEvidence(struct txEdgeBed *evList, char *sourceType, struct hash *bkHash)
/* Input is a list of additional evidence in evList, and a hash
 * full of binKeepers full of txGraphs. */
{
struct txEdgeBed *ev;
for (ev = evList; ev != NULL; ev = ev->next)
    {
    if (specificChrom != NULL && !sameString(ev->chrom, specificChrom))
        continue;
    verbose(2, "Processing %s %s:%d-%d\n", ev->name, ev->chrom, ev->chromStart, ev->chromEnd);
    struct binKeeper *bk = hashMustFindVal(bkHash, ev->chrom);
    struct binElement *bel, *belList = binKeeperFind(bk, ev->chromStart, ev->chromEnd);
    verbose(3, "%s hits %d txg\n", ev->name, slCount(belList));
    for (bel = belList; bel != NULL; bel = bel->next)
        {
	struct txGraph *txg = bel->val;
	verbose(3, "  txg %s %s:%d-%d\n", txg->name, txg->tName, txg->tStart, txg->tEnd);
	if (ev->strand[0] == 0 || ev->strand[0] == '.' 
		||sameString(txg->strand, ev->strand))
	    {
	    addEvidenceToGraph(txg, ev, sourceType);
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
maxBleedOver = optionInt("maxBleedOver", maxBleedOver);
if (argc != 5)
    usage();
txgAddEvidence(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
