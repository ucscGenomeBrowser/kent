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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToAxt - Convert net (and chain) to axt.\n"
  "usage:\n"
  "   netToAxt in.net in.chain tNibDir qNibDir out.axt\n"
  "options:\n"
  "   -xxx=XXX\n"
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
    hashAdd(hash, nameBuf, chain);
    ++count;
    }
lineFileClose(&lf);
printf("read %d chains in %s\n", count, fileName);
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

void subchainT(struct chain *chain, int subStart, int subEnd, 
    struct chain **retSubChain,  struct chain **retChainToFree)
/* Get subchain of chain bounded by subStart-subEnd on 
 * target side.  Return result in *retSubChain.  In some
 * cases this may be the original chain, in which case
 * *retChainToFree is NULL.  When done call chainFree on
 * *retChainToFree.  The score and id fields are not really
 * properly filled in. */
{
struct chain *sub = NULL;
struct boxIn *oldB, *b, *bList = NULL;
int qStart = BIGNUM, qEnd = -BIGNUM;
int tStart = BIGNUM, tEnd = -BIGNUM;

/* Check for easy case. */
if (subStart <= chain->tStart && subEnd >= chain->tEnd)
    {
    *retSubChain = chain;
    *retChainToFree = NULL;
    return;
    }

/* Build new block list and calculate bounds. */
for (oldB = chain->blockList; oldB != NULL; oldB = oldB->next)
    {
    if (oldB->tEnd <= subStart)
        continue;
    if (oldB->tStart >= subEnd)
        break;
    b = CloneVar(oldB);
    if (b->tStart < subStart)
        {
	b->qStart += subStart - b->tStart;
	b->tStart = subStart;
	}
    if (b->tEnd > subEnd)
        {
	b->qEnd -= b->tEnd - subEnd;
	b->tEnd = subEnd;
	}
    slAddHead(&bList, b);
    if (qStart > b->qStart)
        qStart = b->qStart;
    if (qEnd < b->qEnd)
        qEnd = b->qEnd;
    if (tStart > b->tStart)
        tStart = b->tStart;
    if (tEnd < b->tEnd)
        tEnd = b->tEnd;
    }
slReverse(&bList);

/* Make new chain based on old. */
if (bList != NULL)
    {
    AllocVar(sub);
    sub->blockList = bList;
    sub->qName = cloneString(chain->qName);
    sub->qSize = chain->qSize;
    sub->qStrand = chain->qStrand;
    sub->qStart = qStart;
    sub->qEnd = qEnd;
    sub->tName = cloneString(chain->tName);
    sub->tSize = chain->tSize;
    sub->tStart = tStart;
    sub->tEnd = tEnd;
    sub->id = chain->id;
    }
*retSubChain = *retChainToFree = sub;
}

void convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *f)
/* Convert subset of chain as defined by fill to axt. */
{
struct dnaSeq *qSeq;
boolean isRev = (chain->qStrand == '-');
struct chain *subChain, *chainToFree;

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
        reverseComplement(qSeq->dna, qSeq->size);
    uglyf("Read %s:%d %d\n", fill->qName, fill->qStart, fill->qSize);
    }
uglyf("ok1\n");
subchainT(chain, fill->tStart, fill->tStart + fill->tSize, &subChain, &chainToFree);
assert(subChain != NULL);
uglyf("ok2 %p %p %p\n", chain, subChain, chainToFree);
if (chain->tStart != subChain->tStart || chain->qStart != subChain->qStart)
    {
    chainWrite(chain, f);
    chainWrite(subChain, f);
    }
uglyf("ok3\n");
chainFree(&chainToFree);
uglyf("ok4\n");
freeDnaSeq(&qSeq);
}

void rConvert(struct cnFill *fillList, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *qNibDir, struct hash *chainHash, 
	FILE *f)
/* Recursively output chains in net as axt. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        convertFill(fill, tChrom, qChromHash, qNibDir, 
		chainLookup(chainHash, fill->chainId), f);
    if (fill->children)
        rConvert(fill->children, tChrom, qChromHash, qNibDir, chainHash, f);
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

chainHash = chainReadAll(chainName);
while ((net = chainNetRead(lf)) != NULL)
    {
    printf("Processing %s\n", net->name);
    sprintf(path, "%s/%s.nib", tNibDir, net->name);
    tChrom = nibLoadAllMasked( NIB_MASK_MIXED ,path);
    if (tChrom->size != net->size)
        errAbort("Size mismatch on %s.  Net/nib out of sync?", tChrom->name);
    printf("loaded %s\n", path);

    rConvert(net->fillList, tChrom, qChromHash, qNibDir, chainHash, f);
    freeDnaSeq(&tChrom);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionHash(&argc, argv);
if (argc != 6)
    usage();
netToAxt(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
