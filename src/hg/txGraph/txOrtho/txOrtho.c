/* txOrtho - Produce list of shared edges between two transcription graphs in two species. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "localmem.h"
#include "options.h"
#include "geneGraph.h"
#include "txGraph.h"
#include "binRange.h"
#include "chain.h"
#include "chainNet.h"
#include "rbTree.h"
#include "rangeTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txOrtho - Produce list of shared edges between two transcription graphs in two species.\n"
  "usage:\n"
  "   txOrtho in.agx in.chain in.net ortho.agx out.edges\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct indexedChain
/* A chain with indexed blocks. */
    {
    struct chain *chain;	/* Associated chain */
    struct rbTree *blockTree;	/* Tree of blocks. */
    };

void indexedChainSubsetOnT(struct indexedChain *ixc, int subStart, int subEnd, 
    struct chain **retSubChain,  struct chain **retChainToFree)
/* Extract subset of chain that has been indexed. */
{
struct range *r = rangeTreeAllOverlapping(ixc->blockTree, subStart, subEnd);
if (r == NULL)
    *retSubChain = *retChainToFree = NULL;
else
    chainFastSubsetOnT(ixc->chain, r->val, subStart, subEnd, retSubChain, retChainToFree);
}

struct hash *netToBkHash(char *netFile)
/* Read net file into a hash full of binKeepers keyed by chromosome.
 * The binKeepers are full of nets. */
{
struct hash *netHash = hashNew(0);
struct lineFile *lf = lineFileOpen(netFile, TRUE);
struct chainNet *net, *netList = chainNetRead(lf);
for (net = netList; net != NULL; net = net->next)
    {
    if (hashLookup(netHash, net->name))
        errAbort("%s has multiple %s records", netFile, net->name);
    struct binKeeper *bk = binKeeperNew(0, net->size);
    hashAdd(netHash, net->name, bk);
    struct cnFill *fill;
    for(fill=net->fillList; fill != NULL; fill = fill->next)
	binKeeperAdd(bk, fill->tStart, fill->tStart+fill->tSize, fill);
    }
lineFileClose(&lf);
return netHash;                
}

struct hash *allChainsHash(char *fileName)
/* Hash all the chains in a given file by their ids. */
{
struct hash *chainHash = newHash(18);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct chain *chain;
char chainId[20];
struct lm *lm = chainHash->lm;
struct rbTreeNode **stack;

lmAllocArray(lm, stack, 128);
while ((chain = chainRead(lf)) != NULL)
    {
    struct indexedChain *ixc;
    lmAllocVar(lm, ixc);
    ixc->chain = chain;
#ifdef SOON
#endif /* SOON */
    ixc->blockTree = rangeTreeNewDetailed(lm, stack);
    struct cBlock *block;
    for (block = chain->blockList; block != NULL; block = block->next)
	{
        struct range *r = rangeTreeAdd(ixc->blockTree, block->tStart, block->tEnd);
	r->val = block;
	}
    safef(chainId, sizeof(chainId), "%x", chain->id);
    hashAddUnique(chainHash, chainId, ixc);
    }
lineFileClose(&lf);
return chainHash;
}

struct indexedChain *chainFromId(struct hash *chainHash, int id)
/* Given chain ID and hash, return chain. */
{
char chainId[20];
safef(chainId, sizeof(chainId), "%x", id);
return hashMustFindVal(chainHash, chainId);
}

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
	verbose(3, "New binKeeper for %s\n", txg->tName);
	bk = binKeeperNew(0, chrom->minSize);
	hashAdd(bkHash, txg->tName, bk);
	}
    binKeeperAdd(bk, txg->tStart, txg->tEnd, txg);
    }
hashFree(&sizeHash);
return bkHash;
}

struct txGraph *agxForCoordinates(char *chrom, int chromStart, int chromEnd, char strand, 
				    struct hash *orthoChromHash)
/* Get list of graphs that cover a particular region. */
{
struct binElement *beList = NULL, *be = NULL;
struct txGraph *agx = NULL, *agxList = NULL;
struct binKeeper *bk = hashFindVal(orthoChromHash, chrom);
if (bk != NULL)
    {
    beList = binKeeperFind(bk, chromStart, chromEnd);
    for(be = beList; be != NULL; be = be->next)
	{
	agx = be->val;
	if(agx->strand[0] == strand)
	    slSafeAddHead(&agxList, agx);
	}
    slReverse(&agxList);
    slFreeList(&beList);
    }
return agxList;
}

void qChainRangePlusStrand(struct chain *chain, int *retQs, int *retQe)
/* Return range of bases covered by chain on q side on the plus
 * strand. */
{
if (chain == NULL)
    errAbort("Can't find range in null query chain.");
if (chain->qStrand == '-')
    {
    *retQs = chain->qSize - chain->qEnd;
    *retQe = chain->qSize - chain->qStart;
    }
else
    {
    *retQs = chain->qStart;
    *retQe = chain->qEnd;
    }
}

void loadOrthoAgxList(struct txGraph *ag, struct indexedChain *ixc, struct hash *orthoGraphHash,
				   boolean *revRet, struct txGraph **orthoAgListRet)
/** Return the txGraph records in the orhtologous position on the other genome
    as defined by ag and chain. */
{
int qs = 0, qe = 0;
struct txGraph *orthoAgList = NULL; 
struct chain *subChain = NULL, *toFree = NULL;
boolean reverse = FALSE;
char *strand = NULL;
if(ixc != NULL) 
    {
    /* First find the orthologous splicing graph. */
    indexedChainSubsetOnT(ixc, ag->tStart, ag->tEnd, &subChain, &toFree);    
    if(subChain != NULL)
	{
	qChainRangePlusStrand(subChain, &qs, &qe);
	if (subChain->qStrand == '-')
	    reverse = TRUE;
	if(reverse)
	    { 
	    if(ag->strand[0] == '+')
		strand = "-";
	    else
		strand = "+";
	    }
	else
	    strand = ag->strand;
	orthoAgList = agxForCoordinates(subChain->qName, qs, qe, strand[0], orthoGraphHash);
	chainFreeList(&toFree);
	}
    }
*revRet = reverse;
*orthoAgListRet = orthoAgList;
}

int chainBlockCoverage(struct indexedChain *ixc, int start, int end,
		       int* blockStarts, int *blockSizes, int blockCount)
/* Calculate how many of the blocks are covered at both block begin and
 * end by a chain. */
{
int blocksCovered = 0;
int i=0;

/* Find the part of the chain of interest to us. */
struct range *rangeList = rangeTreeAllOverlapping(ixc->blockTree, start, end);

/* Check to see how many of our exons the boxInList contains covers. 
   For each block check to see if the blockStart and blockEnd are 
   found in the boxInList. */
for(i=0; i<blockCount; i++)
    {
    boolean startFound = FALSE;
    int blockStart = blockStarts[i];
    int blockEnd = blockStarts[i] + blockSizes[i];
    struct range *r;

    /* Skip over bits of range list that are no longer relevant. */
    while (rangeList != NULL && rangeList->end <= blockStart)
        rangeList = rangeList->next;

    /* Count up blocks covered on both ends. */
    for (r = rangeList; r != NULL; r = r->next)
	{
	//    CCCCCC  CCCCC       CCCCCC    CCC  CCCC
	//     BBB   BBBB    BBB BBBBBBBB        BBBB
	//     yes    no     no     no      no   yes
	if(r->start <= blockStart && r->end >= blockStart)
	    startFound = TRUE;
	if(startFound && r->start <= blockEnd && r->end >= blockEnd)
	    {
	    blocksCovered++;
	    break;
	    }
	if (r->start > blockEnd)
	    break;
	}
    }
return blocksCovered;
}

boolean betterChain(struct indexedChain *ixc, int start, int end,
		    int* blockStarts, int *blockSizes, int blockCount,
		    struct indexedChain **bestIxc, int *bestCover)
/* Return TRUE if chain is a better fit than bestChain. If TRUE
   fill in bestChain and bestCover. */
{
int blocksCovered = 0;
boolean better = FALSE;

/* Check for easy case. */
if (ixc == NULL)
    return FALSE;
struct chain *chain = ixc->chain;
if(chain == NULL || chain->tStart > end || chain->tStart + chain->tSize < start)
    return FALSE;
    
blocksCovered = chainBlockCoverage(ixc, start, end, blockStarts, blockSizes, blockCount);
if(blocksCovered > (*bestCover))
    {
    *bestIxc = ixc;
    *bestCover = blocksCovered;
    better = TRUE;
    }
return better;
}

void lookForBestChain(struct cnFill *list, int start, int end,
		      int* blockStarts, int *blockSizes, int blockCount,
		      struct hash *chainHash, 
		      struct indexedChain **bestIxc, int *bestCover)
/* Recursively look for the best chain. Best is defined as the chain
   that covers the most number of blocks found in starts and sizes. This
   will be stored in bestChain and number of blocks that it covers will
   be stored in bestCover. */
{
struct cnFill *fill=NULL;
struct cnFill *gap=NULL;

for(fill = list; fill != NULL; fill = fill->next)
    {
    struct indexedChain *ixc = chainFromId(chainHash, fill->chainId);
    betterChain(ixc, start, end, blockStarts, blockSizes, blockCount, bestIxc, bestCover);
    for(gap = fill->children; gap != NULL; gap = gap->next)
	{
	if(gap->children)
	    {
	    lookForBestChain(gap->children, start, end, 
				       blockStarts, blockSizes, blockCount,
				       chainHash, bestIxc, bestCover);
	    }
	}
    }
}

struct cnFill *netFillAt(char *chrom, int start, int end, struct hash *netHash)
/* Get list of highest level fill for net at given position. */
{
struct cnFill *fillList = NULL, *fill;
struct binKeeper *bk = hashFindVal(netHash, chrom);
if (bk != NULL)
    {
    struct binElement *beList = NULL, *be = NULL;
    beList = binKeeperFind(bk, start, end);
    for (be = beList; be != NULL; be = be->next)
        {
	fill = be->val;
	slAddHead(&fillList, fill);
	}
    slFreeList(&beList);
    slReverse(&fillList);
    }
return fillList;
}

struct txGraph *orthoGraphsViaChain(struct txGraph *inGraph, 
	struct indexedChain *ixc, struct hash *orthoGraphHash)
/* Get a list of orthologous graphs in another species using chain to map.
 * The orthologous graphs are simply those that overlap at the exon level on the
 * same strand. */
{
boolean reverse = FALSE;
struct txGraph *orthoGraphList = NULL;
loadOrthoAgxList(inGraph, ixc, orthoGraphHash, &reverse, &orthoGraphList);
return orthoGraphList;
}

struct indexedChain *bestChainForGraph(struct txGraph *inGraph, struct hash *chainHash,
	struct hash *netHash)
/* Find chain that has most block-level overlap with exons in graph. */
{
/* Get appropriate part of nets. */
struct cnFill *fillList = netFillAt(inGraph->tName, 
	inGraph->tStart, inGraph->tEnd, netHash);
if (fillList == NULL)
    return NULL;

/* Create arrays of exon starts and sizes. */
int blockCount = 0;
int *starts, *sizes;
AllocArray(starts, inGraph->edgeCount);
AllocArray(sizes, inGraph->edgeCount);
struct txEdge *edge;
int lastStart = 0;
for (edge = inGraph->edgeList; edge != NULL; edge = edge->next)
    {
    if (edge->type == ggExon)
        {
	int start = starts[blockCount] = inGraph->vertices[edge->startIx].position;
	if (lastStart > start)
	    errAbort("Unsorted edges in graph %s", inGraph->name);
	sizes[blockCount] = inGraph->vertices[edge->endIx].position - start;
	++blockCount;
	}
    }

/* Find chain that most overlaps blocks. */
struct indexedChain *ixc = NULL;
int bestOverlap = 0;
lookForBestChain(fillList, inGraph->tStart, inGraph->tEnd, starts, sizes, blockCount,
	chainHash, &ixc, &bestOverlap);

freeMem(starts);
freeMem(sizes);
return ixc;
}


struct txGraph *findOrthoGraphs(struct txGraph *inGraph, 
	struct indexedChain *ixc, struct hash *orthoGraphHash)
/* Find list of orthologous graphs if any.  Beware the side effect of tweaking
 * some of the fill->next pointers at the highest level of the net. It's expensive
 * to avoid the side effect, and it doesn't bother subsequent calls to this function. */
{
struct txGraph *orthoGraphList = NULL;
if (ixc != NULL)
    {
    struct chain *chain = ixc->chain;
    verbose(3, "Best chain for %s is %s:%d-%d %c %s:%d-%d\n", inGraph->name,
    	chain->tName, chain->tStart, chain->tEnd, chain->qStrand, 
	chain->qName, chain->qStart, chain->qEnd);
    orthoGraphList = orthoGraphsViaChain(inGraph, ixc, orthoGraphHash);
    }
else
    {
    verbose(3, "Couldn't find best chain for %s\n", inGraph->name);
    }
return orthoGraphList;
}

int chainBasesInBlocks(struct chain *chain)
/* Return total number of bases in chain blocks. */
{
int total = 0;
struct cBlock *block;
for (block = chain->blockList; block != NULL; block = block->next)
    total += block->tEnd - block->tStart;
return total;
}


boolean edgeMap(int start, int end, struct indexedChain *ixc,
	int *retStart, int *retEnd, boolean *retRev,
	boolean *retStartExact, boolean *retEndExact,
	int *retCoverage)
/* Map edge through chain. Return FALSE if no map. */
{
struct chain *subChain = NULL, *toFree = NULL;
indexedChainSubsetOnT(ixc, start, end, &subChain, &toFree);
if (!subChain)
    return FALSE;
*retRev = FALSE;
*retStartExact = *retEndExact = FALSE;
qChainRangePlusStrand(subChain, retStart, retEnd);
if (start == subChain->tStart)
    *retStartExact = TRUE;
if (end == subChain->tEnd)
    *retEndExact = TRUE;
if (subChain->qStrand == '-')
    *retRev = TRUE;
*retCoverage = chainBasesInBlocks(subChain);
chainFree(&toFree);
return TRUE;
}

void writeOverlappingEdges(
	enum ggEdgeType edgeType, struct txGraph *inGraph,
	int inStart, int mappedStart, enum ggVertexType startType, boolean startMappedExact, 
	int inEnd, int mappedEnd, enum ggVertexType endType, boolean endMappedExact, 
	struct txGraph *orthoGraph, boolean orthoRev, int mappedCoverage, FILE *f)
/* Write edges of graph that overlap correctly to f */
{
if (startType == ggSoftStart || startType == ggSoftEnd || startMappedExact)
    {
    if (endType == ggSoftStart || endType == ggSoftEnd || endMappedExact)
	{
	struct txEdge *edge;
	for (edge = orthoGraph->edgeList; edge != NULL; edge = edge->next)
	    {
	    if (edge->type == edgeType)
		{
		int orthoStartIx = edge->startIx;
		int orthoStart = orthoGraph->vertices[orthoStartIx].position;
		enum ggVertexType orthoStartType = orthoGraph->vertices[orthoStartIx].type;
		int orthoEndIx = edge->endIx;
		int orthoEnd = orthoGraph->vertices[orthoEndIx].position;
		enum ggVertexType orthoEndType = orthoGraph->vertices[orthoEndIx].type;
		int overlap = rangeIntersection(mappedStart, mappedEnd, orthoStart, orthoEnd);
		if (overlap > 0)
		    {
		    enum ggVertexType rStartType = startType, rEndType = endType;
		    if (orthoRev)
		        {
			rStartType = endType;
			rEndType = startType;
			}
		    if (rStartType == ggSoftStart || rStartType == ggSoftEnd || mappedStart == orthoStart)
			{
			if (rEndType == ggSoftStart || rEndType == ggSoftEnd || mappedEnd == orthoEnd)
			    {
			    /* Compute score that depends on how much start/end have wobbled. */
			    int mappedOverlapScore = 1000 * overlap/(mappedEnd - mappedStart);
			    int orthoOverlapScore = 1000 * overlap/(orthoEnd - orthoStart);
			    int overlapScore = min(mappedOverlapScore, orthoOverlapScore);
			    int coverageScore = 1000 * mappedCoverage/(inEnd - inStart);

			    /* Write out bed fields. */
			    fprintf(f, "%s\t%d\t%d\t", inGraph->tName, inStart, inEnd);
			    fprintf(f, "%s\t", orthoGraph->name);
			    fprintf(f, "%d\t", coverageScore);
			    fprintf(f, "%s\t", inGraph->strand);

			    /* Write out edge and vertex type info. */
			    fprintf(f, "%s\t", ggVertexTypeAsString(startType));
			    fprintf(f, "%s\t", (edgeType == ggExon ? "exon" : "intron"));
			    fprintf(f, "%s\t", ggVertexTypeAsString(endType));

			    /* Write out mouse info. */
			    fprintf(f, "%s\t%d\t%d\t", orthoGraph->tName, mappedStart, mappedEnd);
			    fprintf(f, "%s\t", inGraph->name);
			    fprintf(f, "%d\t", overlapScore);
			    fprintf(f, "%s\t", orthoGraph->strand);
			    fprintf(f, "%s\t", ggVertexTypeAsString(orthoStartType));
			    fprintf(f, "%s\t", ggVertexTypeAsString(orthoEndType));
			    fprintf(f, "%d\t", orthoStart);
			    fprintf(f, "%d\n", orthoEnd);
			    }
			}
		    }
		}
	    }
	}
    }
}

void writeCommonEdges(struct txGraph *inGraph, 
	struct indexedChain *ixc, struct txGraph *orthoGraph, FILE *f)
/* Write out common edges between inGraph and orthoGraph. */
{
struct txEdge *edge;
for (edge = inGraph->edgeList; edge != NULL; edge = edge->next)
    {
    enum ggEdgeType edgeType = edge->type;
    /* Load up end info on exon in other organism. */
    int inStartIx = edge->startIx;
    int inEndIx = edge->endIx;
    int inStart = inGraph->vertices[inStartIx].position;
    int inEnd  = inGraph->vertices[inEndIx].position;
    enum ggVertexType inStartType = inGraph->vertices[inStartIx].type;
    enum ggVertexType inEndType = inGraph->vertices[inEndIx].type;

    int mappedStart, mappedEnd, mappedCoverage;
    boolean mappedRev, mappedStartExact, mappedEndExact;
    if (edgeMap(inStart, inEnd, ixc,  &mappedStart, &mappedEnd,
	    &mappedRev, &mappedStartExact, &mappedEndExact, &mappedCoverage))
	{
	writeOverlappingEdges(edgeType, inGraph,
		inStart, mappedStart, inStartType, mappedStartExact, 
		inEnd, mappedEnd, inEndType, mappedEndExact, orthoGraph, 
		mappedRev, mappedCoverage, f);
	}
    }
}

void writeOrthoEdges(struct txGraph *inGraph, struct hash *chainHash,
	struct hash *netHash, struct hash *orthoGraphHash, FILE *f)
/* Look for orthologous edges, and write any we find. */
{
struct indexedChain *ixc = bestChainForGraph(inGraph, chainHash, netHash);
struct txGraph *orthoGraphList = 
	findOrthoGraphs(inGraph, ixc, orthoGraphHash);
if (orthoGraphList != NULL)
    {
    struct txGraph *orthoGraph;
    for (orthoGraph = orthoGraphList; orthoGraph != NULL; orthoGraph = orthoGraph->next)
        {
	writeCommonEdges(inGraph, ixc, orthoGraph, f);
	}
    verbose(3, "Graph %s maps to %d orthologous graph starting with %s\n", 
    	inGraph->name, slCount(orthoGraphList), orthoGraphList->name);
    }
else
    verbose(4, "No orthologous graph for %s\n", inGraph->name);
}

void txOrtho(char *inAgx, char *inChain, char *inNet, char *orthoAgx, char *outEdges)
/* txOrtho - Produce list of shared edges between two transcription graphs in two species. */
{
/* Load up input and create output file */
struct txGraph *inGraphList = txGraphLoadAll(inAgx);
verbose(1, "Loaded %d input graphs in %s\n", slCount(inGraphList), inAgx);
struct hash *chainHash = allChainsHash(inChain);
verbose(1, "Read %d chains from %s\n", chainHash->elCount, inChain);
struct hash *netHash = netToBkHash(inNet);
verbose(1, "Read %d nets from %s\n", netHash->elCount, inNet);
struct txGraph *orthoGraphList = txGraphLoadAll(orthoAgx);
verbose(1, "Loaded %d ortho graphs in %s\n", slCount(orthoGraphList), orthoAgx);
struct hash *orthoGraphHash = txgIntoKeeperHash(orthoGraphList);
verbose(1, "%d ortho chromosomes/scaffolds\n", orthoGraphHash->elCount);
FILE *f = mustOpen(outEdges, "w");

/* Loop through inGraphList. */
struct txGraph *inGraph;
for (inGraph = inGraphList; inGraph != NULL; inGraph = inGraph->next)
    {
    verbose(2, "Processing %s %s:%d-%d strand %s\n", 
    	inGraph->name, inGraph->tName, inGraph->tStart, inGraph->tEnd,
	inGraph->strand);
    writeOrthoEdges(inGraph, chainHash, netHash, orthoGraphHash, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// pushCarefulMemHandler(3000000000);
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txOrtho(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
