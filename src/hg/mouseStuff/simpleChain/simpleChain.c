/* simpleChain - Stitch psls into chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "chain.h"
#include "dystring.h"

int maxGap=250000;
 
void usage()
/* Explain usage and exit. */
{
errAbort(
  "simpleChain - Stitch psls into chains\n"
  "usage:\n"
  "   simpleChain psls outChains \n"
  "options:\n"
  "   -maxGap=N    defines max gap possible (default 250,000)\n"
  );
}

static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {NULL, 0},
};

struct cseqPair
/* Pair of sequences. */
    {
    struct cseqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct chain *chain;
    };
struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct psl *psl;
    };


void addPslBlocks(struct boxIn **pList, struct psl *psl)
/* Add blocks (gapless subalignments) from psl to block list. */
{
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    struct boxIn *b;
    int size;
    AllocVar(b);
    size = psl->blockSizes[i];
    b->qStart = b->qEnd = psl->qStarts[i];
    b->qEnd += size;
    b->tStart = b->tEnd = psl->tStarts[i];
    b->tEnd += size;
    slAddHead(pList, b);
    }
slReverse(pList);
}

boolean addPslToChain(struct chain *chain, struct psl *psl, int *addedBases)
{
struct boxIn *bList;
struct boxIn *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;
int qStart = psl->qStarts[0];
int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
int tStart = psl->tStarts[0];
int tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];

prevChainBlock = NULL;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if ((chainBlock->tEnd > tStart) || (chainBlock->qEnd > qStart))
	{
	if  (prevChainBlock && !((prevChainBlock->tEnd < tStart) && (prevChainBlock->qEnd < qStart)))
	    return FALSE;
	if  (chainBlock && !((chainBlock->tStart > tEnd) && (chainBlock->qStart > qEnd)))
	    return FALSE;

	break;
	}
    }

    bList = NULL;
    addPslBlocks(&bList, psl);

    if (prevChainBlock == NULL)
	{
	chain->blockList = bList;
	chain->tStart = bList->tStart;
	chain->qStart = bList->qStart;
	}
    else
	{
	if (chainBlock == NULL)
	    {
	    chain->tEnd = tEnd;
	    chain->qEnd = qEnd;
	    }
	prevChainBlock->next = bList;
	}

    for(;bList->next; bList = bList->next)
	*addedBases += bList->tEnd - bList->tStart;
    *addedBases += bList->tEnd - bList->tStart;
    bList->next = chainBlock;

    return TRUE;
}

struct chain *aggregateChains( struct chain *chainIn)
{
struct chain *outChain = NULL;
struct chain *chain1=NULL, *chain2, *chain2Next, *prevChain2;

slSort(&chainIn, chainCmpTarget);

while(chainIn)
    {
    chain1 = chainIn;
    prevChain2 = NULL;
    for(chain2 = chainIn->next; chain2 ;  chain2 = chain2Next)
	{
	    chain2Next = chain2->next;
	    if ((chain1->tStart <= chain2->tStart) && (chain1->tEnd + maxGap > chain2->tStart)
	       && (chain1->qStart <= chain2->qStart) && (chain1->qEnd + maxGap > chain2->qStart) 
	 &&	(chain1->qEnd <= chain2->qStart) && (chain1->tEnd <= chain2->tStart))
		{
		assert(chain1->tStart < chain1->tEnd);
		assert(chain1->qStart < chain1->qEnd);
		assert(chain2->tStart < chain2->tEnd);
		assert(chain2->qStart < chain2->qEnd);
		assert(chain1->qEnd <= chain2->qStart);
		assert(chain1->tEnd <= chain2->tStart);
		chain1->tEnd =  chain2->tEnd;
		chain1->qEnd =  chain2->qEnd;
		chain1->blockList = slCat(chain1->blockList,chain2->blockList);
		chain2->blockList = NULL;
		if (prevChain2)
		    prevChain2->next = chain2Next;
		else
		    chainIn->next = chain2Next;
		//chain2 = NULL;
		}
	    else
		prevChain2 = chain2;
	}
    chainIn = chainIn->next;
    slAddHead(&outChain, chain1);
    }
return outChain;
}

void simpleChain(char *psls,  char *outChainName)
/* simpleChain - Stitch psls into chains. */
{
int lastChainId = -1;
int superCount = 0;
struct psl *prevPsl, *nextPsl;
struct psl *fakePslList;
int jj;
int deletedBases, addedBases;
FILE *outChains = mustOpen(outChainName, "w");
struct cseqPair *cspList = NULL, *csp = NULL;
struct seqPair *spList = NULL, *sp;
struct lineFile *pslLf = pslFileOpen(psls);
struct dyString *dy = newDyString(512);
struct psl *psl;
struct hash *pslHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct hash *chainHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct chain *chain, *chainList = NULL;
struct boxIn *block , *nextBlock = NULL, *prevBlock = NULL;
int count;

count = 0;
while ((psl = pslNext(pslLf)) != NULL)
    {
    assert((psl->strand[1] == 0) || (psl->strand[1] == '+'));
    count++;
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", psl->qName, psl->strand[0], psl->tName);
    sp = hashFindVal(pslHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pslHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(psl->qName);
	sp->tName = cloneString(psl->tName);
	sp->qStrand = psl->strand[0];
	}

    slAddHead(&sp->psl, psl);
    }
lineFileClose(&pslLf);
verbose(1,"read in  %d psls\n",count);

addedBases = deletedBases = 0;
for(sp = spList; sp; sp = sp->next)
    {
    int count = 0;
    /* make sure we have a chainList */
    chainList = NULL;

    AllocVar(csp);
    slAddHead(&cspList, csp);
    csp->qName = cloneString(sp->psl->qName);
    csp->tName = cloneString(sp->psl->tName);
    csp->qStrand = sp->psl->strand[0];
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", csp->qName, csp->qStrand, csp->tName);
    hashAddSaveName(chainHash, dy->string, csp, &csp->name);

    for(psl = sp->psl; psl ;  psl = nextPsl)
	{
	nextPsl = psl->next;
	sp->psl  = nextPsl;

	int qStart = psl->qStarts[0];
	int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	int tStart = psl->tStarts[0];
	int tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];

	assert(tEnd > tStart);
	assert(qEnd > qStart);
	/* check for overlap */
	for(chain = chainList; chain ;  chain = chain->next)
	    {
	    if ((((tStart < chain->tEnd) && (tEnd > chain->tStart)) &&
		 (qStart < chain->qEnd) && (qEnd > chain->qStart)))
		    goto out;
	    }
	count++;

	AllocVar(chain);
	chain->tStart = psl->tStarts[0];
	chain->qStart = psl->qStarts[0];
	chain->tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	chain->qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	chain->tSize = psl->tSize;
	chain->qSize = psl->qSize;
	chain->qName = cloneString(psl->qName);
	chain->tName = cloneString(psl->tName);
	chain->qStrand = psl->strand[0];
	chain->id = ++lastChainId;

	if (!addPslToChain(chain, psl, &addedBases))
	    errAbort("new ");
	slAddHead(&chainList, chain);

out:
	freez(&psl);
	}

    for(chain = aggregateChains(chainList); chain ;  chain = chain->next)
	chainWrite(chain, outChains);
    }

fclose(outChains);

dyStringFree(&dy);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxGap = optionInt("maxGap", maxGap);
simpleChain(argv[1], argv[2]);
return 0;
}
