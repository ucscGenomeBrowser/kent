/* netToBedWithId - Convert net (and chain) to bed. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"
#include "chainNet.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "bed.h"


boolean qChain = FALSE;  /* Do chain from query side. */
int maxGap = 5000;
int minSpan = 5000;
double minAli = 0.30;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToBedWithId - Convert net (and chain) to bed with base identity in score.\n"
  "usage:\n"
  "   netToBedWithId in.net in.chain tNibDir qNibDir out.bed\n"
  "options:\n"
  "   -qChain - net is with respect to the q side of chains.\n"
  "   -maxGap=N - maximum size of gap before breaking. Default %d\n"
  "   -minSpan=N - minimum spanning size to output. Default %d\n"
  "   -minAli=N.N - minimum percentage of bases in alignments. Default %f\n"
  ,  maxGap, minSpan, minAli
  );
}

static struct optionSpec options[] = {
   {"qChain", OPTION_BOOLEAN},
   {"maxGap", OPTION_INT},
   {"minSpan", OPTION_INT},
   {"minAli", OPTION_FLOAT},
   {"bed", OPTION_BOOLEAN},
   {NULL, 0},
};


void writeGaps(struct chain *chain, FILE *f)
/* Write gaps to simple two column file. */
{
struct cBlock *a, *b;
a = chain->blockList;
for (b = a->next; b != NULL; b = b->next)
    {
    fprintf(f, "%d\t%d\n", b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
}

int countMatch(char *a, char *b, int size)
/* Return number of matching characters in a/b. */
{
int i, count=0;
for (i=0; i<size; ++i)
    if (a[i] == b[i])
        ++count;
return count;
}

static struct bed *bedFromBlocks(
	struct chain *chain,
	struct cBlock *startB, struct cBlock *endB,
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, double minAli)
/* Convert a list of blocks (guaranteed not to have inserts in both
 * strands between them) to a bed. */
{
struct cBlock *b, *lastB = startB;
int size;
struct bed *bed;
int aliBases = 0, matchingBases = 0;
double ratio;
int coverBases;

for (b = startB; b != endB; b = b->next)
    {
    size = b->tEnd - b->tStart;
    aliBases += size;
    matchingBases += countMatch(qSeq->dna + b->qStart - qOffset, 
    	tSeq->dna + b->tStart - tOffset, size);
    lastB = b;
    }
coverBases = lastB->tEnd - startB->tStart;
ratio = (double)aliBases/(double)coverBases;
if (ratio < minAli)
    return NULL;
AllocVar(bed);
bed->chrom = cloneString(chain->tName);
bed->chromStart = startB->tStart;
bed->chromEnd = lastB->tEnd;
bed->name = cloneString(chain->qName);
if (aliBases > 0)
    bed->score = round(1000 * (double)matchingBases/(double)aliBases);
bed->strand[0] = chain->qStrand;
return bed;
}

struct bed *chainToBed(struct chain *chain, 
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, int maxGap)
{
struct cBlock *startB = chain->blockList, *a = NULL, *b;
struct bed *bedList = NULL, *bed;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    if (a != NULL)
        {
	int dq = b->qStart - a->qEnd;
	int dt = b->tStart - a->tEnd;
	if (dt > maxGap || dq > maxGap)
	    {
	    bed = bedFromBlocks(chain, startB, b, 
	    	qSeq, qOffset, tSeq, tOffset, minAli);
	    if (bed != NULL)
		slAddHead(&bedList, bed);
	    startB = b;
	    }
	}
    a = b;
    }
bed = bedFromBlocks(chain, startB, NULL, 
	qSeq, qOffset, tSeq, tOffset, minAli);
if (bed != NULL)
    slAddHead(&bedList, bed);
slReverse(&bedList);
return bedList;
}

void writeBedFromChain(struct chain *chain, struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, FILE *f)
/* Write out bed's that correspond to chain. */
{
struct bed *bed, *bedList;


bedList = chainToBed(chain, qSeq, qOffset, tSeq, tOffset, maxGap);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromEnd - bed->chromStart >= minSpan)
	bedOutputN(bed, 6, f, '\t', '\n');
    }
bedFreeList(&bedList);
}

void convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *f)
/* Convert subset of chain as defined by fill to bed. */
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;
int qOffset;

/* Get query sequence fragment. */
    {
    struct nibInfo *nib = nibInfoFromCache(qChromHash, nibDir, fill->qName);
    qSeq = nibLoadPart(nib->fileName, 
    	fill->qStart, fill->qSize);
    if (isRev)
	{
        reverseComplement(qSeq->dna, qSeq->size);
	qOffset = nib->size - (fill->qStart + fill->qSize);
	}
    else
	qOffset = fill->qStart;
    }
chainSubsetOnT(chain, fill->tStart, fill->tStart + fill->tSize, 
	&subChain, &chainToFree);
assert(subChain != NULL);
writeBedFromChain(subChain, qSeq, qOffset, tChrom, 0, f);
chainFree(&chainToFree);
freeDnaSeq(&qSeq);
}

void rConvert(struct cnFill *fillList, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *qNibDir, struct hash *chainHash, 
	FILE *f)
/* Recursively output chains in net as bed. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, tChrom, qChromHash, qNibDir, 
		chainLookup(chainHash, fill->chainId), f);
    if (fill->children)
        rConvert(fill->children, tChrom, qChromHash, qNibDir, chainHash, 
		f);
    }
}

#define maxChainId (256*1024*1024)

Bits *findUsedIds(char *netFileName)
/* Create a bit array with 1's corresponding to
 * chainId's used in net file. */
{
struct lineFile *lf = lineFileOpen(netFileName, TRUE);
Bits *bits = bitAlloc(maxChainId);
struct chainNet *net;
while ((net = chainNetRead(lf)) != NULL)
    {
    chainNetMarkUsed(net, bits, maxChainId);
    chainNetFree(&net);
    }
lineFileClose(&lf);
return bits;
}


void netToBedWithId(char *netName, char *chainName, char *tNibDir, char *qNibDir, char *bedName)
/* netToBedWithId - Convert net (and chain) to bed.. */
{
Bits *usedBits = findUsedIds(netName);
struct hash *chainHash;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netName, TRUE);
FILE *f = mustOpen(bedName, "w");
struct dnaSeq *tChrom = NULL;
struct hash *qChromHash = hashNew(0);
char path[512];

chainHash = chainReadUsedSwap(chainName, qChain, usedBits);
bitFree(&usedBits);
while ((net = chainNetRead(lf)) != NULL)
    {
    fprintf(stderr, "Processing %s\n", net->name);
    sprintf(path, "%s/%s.nib", tNibDir, net->name);
    tChrom = nibLoadAll(path);
    if (tChrom->size != net->size)
        errAbort("Size mismatch on %s.  Net/nib out of sync or possibly nib dirs swapped?", 
		tChrom->name);
    fprintf(stderr, "loaded %s\n", path);

    rConvert(net->fillList, tChrom, qChromHash, qNibDir, chainHash, f);
    freeDnaSeq(&tChrom);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
qChain = optionExists("qChain");
maxGap = optionInt("maxGap", maxGap);
minSpan = optionInt("minSpan", minSpan);
minAli = 0.01 * optionFloat("minAli", minAli*100);
if (argc != 6)
    usage();
netToBedWithId(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
