/* netToAxt - Convert net (and chain) to axt.. */

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
#include "chainToAxt.h"
#include "axt.h"
#include "nibTwo.h"


boolean qChain = FALSE;  /* Do chain from query side. */
int maxGap = 100;
boolean splitOnInsert = TRUE;

static struct optionSpec optionSpecs[] = {
    {"qChain", OPTION_BOOLEAN},
    {"maxGap", OPTION_INT},
    {"gapOut", OPTION_STRING},
    {"noSplit", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToAxt - Convert net (and chain) to axt.\n"
  "usage:\n"
  "   netToAxt in.net in.chain target.2bit query.2bit out.axt\n"
  "note:\n"
  "   directories full of .nib files (an older format)\n"
  "   may also be used in place of target.2bit and query.2bit.\n"
  "options:\n"
  "   -qChain - net is with respect to the q side of chains.\n"
  "   -maxGap=N - maximum size of gap before breaking. Default %d\n"
  "   -gapOut=gap.tab - Output gap sizes to file\n"
  "   -noSplit - Don't split chain when there is an insertion of another chain\n"
  ,  maxGap
  );
}

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

void writeAxtFromChain(struct chain *chain, struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, FILE *f, FILE *gapFile)
/* Write out axt's that correspond to chain. */
{
struct axt *axt, *axtList;

if (gapFile != NULL)
    writeGaps(chain, gapFile);
axtList = chainToAxt(chain, qSeq, qOffset, tSeq, tOffset, maxGap, BIGNUM);
verbose(9, "%d axts\n", slCount(axtList));
for (axt = axtList; axt != NULL; axt = axt->next)
    axtWrite(axt, f);
axtFreeList(&axtList);
}

void writeChainPart(struct dnaSeq *tChrom,
	struct nibTwoCache *qNtc, char *nibDir,
	struct chain *chain, int tStart, int tEnd, FILE *f, FILE *gapFile)
/* write out axt's from subset of chain */
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;
int fullSeqSize;
int qStart;

chainSubsetOnT(chain, tStart, tEnd, &subChain, &chainToFree);
if (subChain == NULL)
    errAbort("null subchain in chain ID %d\n", chain->id);

/* Get query sequence fragment. */
nibTwoCacheSeqPart(qNtc, chain->qName, 1, 1, &fullSeqSize);
qStart = (isRev ? fullSeqSize - subChain->qEnd : subChain->qStart);
qSeq = nibTwoCacheSeqPart(qNtc, subChain->qName, qStart, 
                                subChain->qEnd - subChain->qStart, NULL);
if (isRev)
    reverseComplement(qSeq->dna, qSeq->size);

verbose(9, "fill chain id, subchain %d %s %d %d %c qOffset=%d\n", 
                subChain->id, subChain->qName,
                tStart, tEnd, subChain->qStrand, qStart);
writeAxtFromChain(subChain, qSeq, subChain->qStart, tChrom, 0, f, gapFile);
chainFree(&chainToFree);
freeDnaSeq(&qSeq);
}

struct cnFill *nextGapWithInsert(struct cnFill *gapList)
/* Find next in list that has a non-empty child.   */
{
struct cnFill *gap;
for (gap = gapList; gap != NULL; gap = gap->next)
    {
    if (gap->children != NULL)
        break;
    }
return gap;
}

void splitWrite(struct cnFill *fill, struct dnaSeq *tChrom,
	struct nibTwoCache *qNtc, char *qNibDir,
	struct chain *chain, FILE *f, FILE *gapFile)
/* Split chain into pieces if it has inserts.  Write out
 * each piece. */
{
int tStart = fill->tStart;
struct cnFill *child = fill->children;

for (;;)
    {
    child = nextGapWithInsert(child);
    if (child == NULL)
        break;
    writeChainPart(tChrom, qNtc, qNibDir,
                        chain, tStart, child->tStart, f, gapFile);
    tStart = child->tStart + child->tSize;
    child = child->next;
    }
writeChainPart(tChrom, qNtc, qNibDir, 
                        chain, tStart, fill->tStart + fill->tSize, f, gapFile);
}

void convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
        struct nibTwoCache *qNtc, char *qNibDir,
	struct chain *chain, FILE *f, FILE *gapFile)
/* Convert subset of chain as defined by fill to axt. */
{
if (splitOnInsert)
    splitWrite(fill, tChrom, qNtc, qNibDir, chain, f, gapFile);
else
    writeChainPart(tChrom, qNtc, qNibDir, 
                chain, fill->tStart, fill->tStart + fill->tSize, f, gapFile);
}

void rConvert(struct cnFill *fillList, struct dnaSeq *tChrom,
	struct nibTwoCache *qNtc, char *qNibDir, struct hash *chainHash, 
	FILE *f, FILE *gapFile)
/* Recursively output chains in net as axt. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, tChrom, qNtc, qNibDir, 
		chainLookup(chainHash, fill->chainId), f, gapFile);
    if (fill->children)
        rConvert(fill->children, tChrom, qNtc, qNibDir, chainHash, 
		f, gapFile);
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

void netToAxt(char *netName, char *chainName, char *tNibDir, char *qNibDir, char *axtName)
/* netToAxt - Convert net (and chain) to axt.. */
{
Bits *usedBits = findUsedIds(netName);
struct hash *chainHash;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netName, TRUE);
FILE *f = mustOpen(axtName, "w");
struct dnaSeq *tChrom = NULL;
struct nibTwoCache *qNtc = nibTwoCacheNew(qNibDir);
char *gapFileName = optionVal("gapOut", NULL);
FILE *gapFile = NULL;

if (gapFileName)
    gapFile = mustOpen(gapFileName, "w");
lineFileSetMetaDataOutput(lf, f);
chainHash = chainReadUsedSwap(chainName, qChain, usedBits);
bitFree(&usedBits);
while ((net = chainNetRead(lf)) != NULL)
    {
    verbose(1, "Processing %s\n", net->name);
    tChrom = nibTwoLoadOne(tNibDir, net->name);
    if (tChrom->size != net->size)
	errAbort("Size mismatch on %s.  Net/nib out of sync or possibly nib dirs swapped?", 
		tChrom->name);
    rConvert(net->fillList, tChrom, qNtc, qNibDir, chainHash, f, gapFile);
    freeDnaSeq(&tChrom);
    chainNetFree(&net);
    }
nibTwoCacheFree(&qNtc);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, optionSpecs);
qChain = optionExists("qChain");
maxGap = optionInt("maxGap", maxGap);
splitOnInsert = !optionExists("noSplit");
if (argc != 6)
    usage();
netToAxt(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
