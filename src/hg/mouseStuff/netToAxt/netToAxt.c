/* netToAxt - Convert net (and chain) to axt.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"
#include "chainNet.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "axt.h"

boolean qChain = FALSE;  /* Do chain from query side. */
int maxGap = 10000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToAxt - Convert net (and chain) to axt.\n"
  "usage:\n"
  "   netToAxt in.net in.chain tNibDir qNibDir out.axt\n"
  "options:\n"
  "   -qChain - net is with respect to the q side of chains.\n"
  "   -maxGap=N - maximum size of gap before breaking. Default 10000\n"
  "   -gapOut=gap.tab - Output gap sizes to file\n"
  );
}

struct hash *chainReadAll(char *fileName)
/* Read chains into a hash keyed by id. */
{
char nameBuf[16];
struct hash *hash = hashNew(18);
struct chain *chain;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int count = 0;

while ((chain = chainRead(lf)) != NULL)
    {
    sprintf(nameBuf, "%x", chain->id);
    if (hashLookup(hash, nameBuf))
        errAbort("Duplicate chain %d ending line %d of %s", 
		chain->id, lf->lineIx, lf->fileName);
    if (qChain)
        chainSwap(chain);
    hashAdd(hash, nameBuf, chain);
    ++count;
    }
lineFileClose(&lf);
fprintf(stderr, "read %d chains in %s\n", count, fileName);
return hash;
}

struct chain *chainLookup(struct hash *hash, int id)
/* Find chain in hash. */
{
char nameBuf[16];
sprintf(nameBuf, "%x", id);
return hashMustFindVal(hash, nameBuf);
}

struct nibInfo
/* Info on a nib file. */
    {
    struct nibInfo *next;
    char *fileName;	/* Name of nib file. */
    int size;		/* Number of bases in nib. */
    FILE *f;		/* Open file. */
    };


struct axt *axtFromBlocks(
	struct chain *chain,
	struct boxIn *startB, struct boxIn *endB,
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset)
/* Convert a list of blocks (guaranteed not to have inserts in both
 * strands between them) to an axt. */
{
int symCount = 0;
int dq, dt, blockSize = 0, symIx = 0;
struct boxIn *b, *a = NULL;
struct axt *axt;
char *qSym, *tSym;

/* Make a pass through figuring out how big output will be. */
for (b = startB; b != endB; b = b->next)
    {
    if (a != NULL)
        {
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	symCount += dq + dt;
	}
    blockSize = b->qEnd - b->qStart;
    symCount += blockSize;
    a = b;
    }

/* Allocate axt and fill in most fields. */
AllocVar(axt);
axt->qName = cloneString(chain->qName);
axt->qStart = startB->qStart;
axt->qEnd = a->qEnd;
axt->qStrand = chain->qStrand;
axt->tName = cloneString(chain->tName);
axt->tStart = startB->tStart;
axt->tEnd = a->tEnd;
axt->tStrand = '+';
axt->symCount = symCount;
axt->qSym = qSym = needLargeMem(symCount+1);
qSym[symCount] = 0;
axt->tSym = tSym = needLargeMem(symCount+1);
tSym[symCount] = 0;

/* Fill in symbols. */
a = NULL;
for (b = startB; b != endB; b = b->next)
    {
    if (a != NULL)
        {
	dq = b->qStart - a->qEnd;
	dt = b->tStart - a->tEnd;
	if (dq == 0)
	    {
	    memset(qSym+symIx, '-', dt);
	    memcpy(tSym+symIx, tSeq->dna + a->tEnd - tOffset, dt);
	    symIx += dt;
	    }
	else
	    {
	    assert(dt == 0);
	    memset(tSym+symIx, '-', dq);
	    memcpy(qSym+symIx, qSeq->dna + a->qEnd - qOffset, dq);
	    symIx += dq;
	    }
	}
    blockSize = b->qEnd - b->qStart;
    memcpy(qSym+symIx, qSeq->dna + b->qStart - qOffset, blockSize);
    memcpy(tSym+symIx, tSeq->dna + b->tStart - tOffset, blockSize);
    symIx += blockSize;
    a = b;
    }
assert(symIx == symCount);

/* Fill in score and return. */
axt->score = axtScoreDnaDefault(axt);
return axt;
}

struct axt *axtListFromChain(struct chain *chain, 
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset)
/* Convert a chain to a list of axt's. */
{
struct boxIn *startB = chain->blockList, *endB, *a = NULL, *b;
struct axt *axtList = NULL, *axt;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    if (a != NULL)
        {
	int dq = b->qStart - a->qEnd;
	int dt = b->tStart - a->tEnd;
	if ((dq > 0 && dt > 0) || dt > maxGap || dq > maxGap)
	    {
	    axt = axtFromBlocks(chain, startB, b, qSeq, qOffset, tSeq, tOffset);
	    slAddHead(&axtList, axt);
	    startB = b;
	    }
	}
    a = b;
    }
axt = axtFromBlocks(chain, startB, NULL, qSeq, qOffset, tSeq, tOffset);
slAddHead(&axtList, axt);
slReverse(&axtList);
return axtList;
}

void writeGaps(struct chain *chain, FILE *f)
/* Write gaps to simple two column file. */
{
struct boxIn *a, *b;
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
axtList = axtListFromChain(chain, qSeq, qOffset, tSeq, tOffset);
for (axt = axtList; axt != NULL; axt = axt->next)
    axtWrite(axt, f);
axtFreeList(&axtList);
}

void convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *f, FILE *gapFile)
/* Convert subset of chain as defined by fill to axt. */
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;
int qOffset;

/* Get query sequence fragment. */
    {
    struct nibInfo *nib = hashFindVal(qChromHash, fill->qName);
    if (nib == NULL)
        {
	char path[512];
	AllocVar(nib);
	sprintf(path, "%s/%s.nib", nibDir, fill->qName);
	nib->fileName = cloneString(path);
	nibOpenVerify(path, &nib->f, &nib->size);
	hashAdd(qChromHash, fill->qName, nib);
	}
    qSeq = nibLoadPartMasked(NIB_MASK_MIXED, nib->fileName, 
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
writeAxtFromChain(subChain, qSeq, qOffset, tChrom, 0, f, gapFile);
chainFree(&chainToFree);
freeDnaSeq(&qSeq);
}

void rConvert(struct cnFill *fillList, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *qNibDir, struct hash *chainHash, 
	FILE *f, FILE *gapFile)
/* Recursively output chains in net as axt. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, tChrom, qChromHash, qNibDir, 
		chainLookup(chainHash, fill->chainId), f, gapFile);
    if (fill->children)
        rConvert(fill->children, tChrom, qChromHash, qNibDir, chainHash, 
		f, gapFile);
    }
}

void netToAxt(char *netName, char *chainName, char *tNibDir, char *qNibDir, char *axtName)
/* netToAxt - Convert net (and chain) to axt.. */
{
struct hash *chainHash;
struct chainNet *net;
struct lineFile *lf = lineFileOpen(netName, TRUE);
FILE *f = mustOpen(axtName, "w");
struct dnaSeq *tChrom = NULL;
struct hash *qChromHash = hashNew(0);
char path[512];
char *gapFileName = optionVal("gapOut", NULL);
FILE *gapFile = NULL;

if (gapFileName)
    gapFile = mustOpen(gapFileName, "w");
chainHash = chainReadAll(chainName);
while ((net = chainNetRead(lf)) != NULL)
    {
    fprintf(stderr, "Processing %s\n", net->name);
    sprintf(path, "%s/%s.nib", tNibDir, net->name);
    tChrom = nibLoadAllMasked( NIB_MASK_MIXED ,path);
    if (tChrom->size != net->size)
        errAbort("Size mismatch on %s.  Net/nib out of sync or possibly nib dirs swapped?", 
		tChrom->name);
    fprintf(stderr, "loaded %s\n", path);

    rConvert(net->fillList, tChrom, qChromHash, qNibDir, chainHash, f, gapFile);
    freeDnaSeq(&tChrom);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionHash(&argc, argv);
qChain = optionExists("qChain");
maxGap = optionInt("maxGap", maxGap);
if (argc != 6)
    usage();
netToAxt(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
