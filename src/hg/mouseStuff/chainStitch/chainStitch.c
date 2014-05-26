/* chainStitch - Stitch psls into chains. */

/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
  "chainStitch - Stitch psls into chains\n"
  "usage:\n"
  "   chainStitch psls chains outChains outFound outNotFound\n"
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


void addPslBlocks(struct cBlock **pList, struct psl *psl)
/* Add blocks (gapless subalignments) from psl to block list. */
{
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    struct cBlock *b;
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

int boxInCmpBoth(const void *va, const void *vb)
/* Compare to sort based on query, then target. */
{
const struct cBlock *a = *((struct cBlock **)va);
const struct cBlock *b = *((struct cBlock **)vb);
int dif;
dif = a->tStart - b->tStart;
if (dif == 0)
    dif = a->qStart - b->qStart;
return dif;
}

int seqPairCmp(const void *va, const void *vb)
/* Compare to sort based on tName,qName. */
{
const struct seqPair *a = *((struct seqPair **)va);
const struct seqPair *b = *((struct seqPair **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = strcmp(a->qName, b->qName);
if (dif == 0)
    dif = (int)a->qStrand - (int)b->qStrand;
return dif;
}

boolean checkChainRange(struct chain *chain, int qs, int qe, int ts, int te)
{
struct cBlock *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;

prevChainBlock = NULL;
//if ((te < chain->tStart) && (qe < chain->qStart))
    //return FALSE;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if (((ts < chainBlock->tEnd) && (te > chainBlock->tStart)) ||
	 (qs < chainBlock->qEnd) && (qe > chainBlock->qStart))
	return TRUE;
    }
    return FALSE;
}

boolean deleteChainRange(struct chain *chain, int qs, int qe, int ts, int te, int *deletedBases)
{
struct cBlock *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;

prevChainBlock = NULL;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if (((ts <= chainBlock->tEnd) && (te > chainBlock->tStart)) &&
	 (qs <= chainBlock->qEnd) && (qe > chainBlock->qStart))
	{
	while(chainBlock && ((chainBlock->tStart < te) || (chainBlock->qStart < qe)))
	    {
	    nextChainBlock = chainBlock->next;
	    *deletedBases += chainBlock->tEnd - chainBlock->tStart;
	    if (prevChainBlock == NULL)
		chain->blockList = nextChainBlock;
	    else
		prevChainBlock->next = nextChainBlock;
	    freez(&chainBlock);
	    chainBlock = nextChainBlock;
	    }

	return TRUE;
	}
    }
    return FALSE;
}

boolean addPslToChain(struct chain *chain, struct psl *psl, int *addedBases)
{
struct cBlock *bList;
struct cBlock *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;
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

    //slSort(&chain->blockList,  boxInCmpBoth);
    return TRUE;
}

void checkInChains(struct psl **pslList, struct chain **chainList, FILE *outFound, int *addedBases)
{
struct chain *nextChain, *prevChain;
struct chain *chain;
struct psl *psl;
struct psl *prevPsl, *nextPsl;

prevPsl = NULL;
for(psl = *pslList; psl ;  psl = nextPsl)
    {
    nextPsl = psl->next;
    int qStart = psl->qStarts[0];
    int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
    int tStart = psl->tStarts[0];
    int tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];

    assert(tEnd > tStart);
    assert(qEnd > qStart);
    prevChain = 0;
    for(chain = *chainList; chain ; prevChain = chain , chain = nextChain)
	{
	nextChain = chain->next;
	if (((tStart < chain->tEnd) && (tEnd > chain->tStart)) &&
	     (qStart < chain->qEnd) && (qEnd > chain->qStart))
	    {
	    if ( addPslToChain(chain, psl, addedBases))
		{
		pslTabOut(psl, outFound);
		}

	    if (prevPsl != NULL)
		prevPsl->next = nextPsl;
	    else
		*pslList = nextPsl;
		
	    freez(&psl);
	    break;
	    }
	}
    if (chain == NULL)
	prevPsl = psl;
    
    }
}

void checkAfterChains(struct psl **pslList, struct chain **chainList, FILE *outFound, int *addedBases)
{
struct cBlock *block;
struct chain *nextChain, *prevChain;
struct chain *chain;
struct psl *psl;
struct psl *prevPsl, *nextPsl;
prevPsl = NULL;
for(psl = *pslList; psl ;  psl = nextPsl)
    {
    nextPsl = psl->next;
    int qStart = psl->qStarts[0];
    int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
    int tStart = psl->tStarts[0];
    int tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];

    prevChain = 0;
    for(chain = *chainList; chain ; prevChain = chain , chain = nextChain)
	{
	nextChain = chain->next;
	if (((tStart <= chain->tEnd + maxGap) && (tStart > chain->tEnd)) &&
	     (qStart <= chain->qEnd + maxGap) && (qStart > chain->qEnd))
	    {
	    if (!addPslToChain(chain, psl, addedBases))
		errAbort("after chains");

	    if (prevPsl != NULL)
		prevPsl->next = nextPsl;
	    else
		*pslList = nextPsl;
		
	    pslTabOut(psl, outFound);
	    freez(&psl);
	    break;
	    }
	}
    if (chain == NULL)
	prevPsl = psl;
    
    }
}

struct chain *aggregateChains( struct chain *chainIn)
{
struct chain *outChain = NULL;
struct chain *chain1, *chain2, *chain1Next;

//printf("%s%c%s\n",chainIn->qName, chainIn->qStrand, chainIn->tName);
for(chain1 = chainIn; chain1 ; chain1 = chain1Next)
    {
    chain1Next = chain1->next;
    for(chain2 = chain1Next; chain2 ; chain2 = chain2->next)
	{
	    if ((chain1->tStart <= chain2->tStart) && (chain1->tEnd + maxGap > chain2->tStart)
	       && (chain1->qStart <= chain2->qStart) && (chain1->qEnd + maxGap > chain2->qStart) &&
		(chain1->qEnd < chain2->qStart) && (chain1->tEnd < chain2->tStart))
		{
		assert(chain1->tStart < chain1->tEnd);
		assert(chain1->qStart < chain1->qEnd);
		assert(chain2->tStart < chain2->tEnd);
		assert(chain2->qStart < chain2->qEnd);
		assert(chain1->qEnd < chain2->qStart);
		assert(chain1->qEnd < chain2->qStart);
		chain2->tStart = chain1->tStart;
		chain2->qStart = chain1->qStart;
		chain2->tEnd = max(chain1->tEnd, chain2->tEnd);
		chain2->qEnd = max(chain1->qEnd, chain2->qEnd);
		chain2->blockList = slCat(chain1->blockList,chain2->blockList);
	//	slSort(&chain2->blockList,  boxInCmpBoth);
		freez(&chain1);
		break;
		}
	    else if ((chain1->tStart >= chain2->tEnd) && (chain1->tStart - maxGap < chain2->tEnd)
		&& (chain1->qStart >= chain2->qEnd) && (chain1->qStart - maxGap < chain2->qEnd))
		{
		errAbort("shouldn't get here\n");
		chain2->tEnd = chain1->tEnd;
		chain2->qEnd = chain1->qEnd;
		chain2->blockList = slCat(chain2->blockList,chain1->blockList);
		freez(&chain1);
		break;
		}
	}

    if (chain2 == NULL)
	{
	slAddHead(&outChain, chain1);
	}
    }
return outChain;
}

void checkBeforeChains(struct psl **pslList, struct chain **chainList, FILE *outFound, int *addedBases)
{
struct cBlock *block;
struct chain *nextChain, *prevChain;
struct chain *chain;
struct psl *psl;
struct psl *prevPsl, *nextPsl;
prevPsl = NULL;
for(psl = *pslList; psl ;  psl = nextPsl)
    {
    nextPsl = psl->next;
    int qStart = psl->qStarts[0];
    int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
    int tStart = psl->tStarts[0];
    int tEnd = psl->tStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];

    prevChain = 0;
    for(chain = *chainList; chain ; prevChain = chain , chain = nextChain)
	{
	nextChain = chain->next;
	if (((tStart > chain->tStart - maxGap) && (tEnd < chain->tStart)) &&
	     (qStart > chain->qStart - maxGap) && (qEnd < chain->qStart))
	    {
	    if (!addPslToChain(chain, psl, addedBases))
		errAbort("before chains");

	    if (prevPsl != NULL)
		prevPsl->next = nextPsl;
	    else
		*pslList = nextPsl;
		
	    pslTabOut(psl, outFound);
	    freez(&psl);
	    break;
	    }
	}
    if (chain == NULL)
	prevPsl = psl;
    
    }
}

void chainStitch(char *psls, char *chains, char *outChainName, char *outFoundName, char *outNotFoundName)
/* chainStitch - Stitch psls into chains. */
{
int lastChainId = -1;
struct psl *prevPsl, *nextPsl;
struct psl *fakePslList;
int jj;
int deletedBases, addedBases;
FILE *outFound = mustOpen(outFoundName, "w");
FILE *outNotFound = mustOpen(outNotFoundName, "w");
FILE *outChains = mustOpen(outChainName, "w");
struct lineFile *chainsLf = lineFileOpen(chains, TRUE);
struct cseqPair *cspList = NULL, *csp;
struct seqPair *spList = NULL, *sp;
struct lineFile *pslLf = pslFileOpen(psls);
struct dyString *dy = newDyString(512);
struct psl *psl;
struct hash *pslHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct hash *chainHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct chain *chain, *chainList = NULL;
struct cBlock *block , *nextBlock = NULL, *prevBlock = NULL;
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
printf("read in  %d psls\n",count);

for(sp = spList; sp; sp = sp->next)
    slReverse(&sp->psl);
    //slSort(&sp->psl, pslCmpTarget);

count = 0;
while ((chain = chainRead(chainsLf)) != NULL)
    {
    if (chain->id > lastChainId)
	lastChainId = chain->id;
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", chain->qName, chain->qStrand, chain->tName);
    csp = hashFindVal(chainHash, dy->string);
    if (csp == NULL)
        {
	AllocVar(csp);
	slAddHead(&cspList, csp);
	hashAddSaveName(chainHash, dy->string, csp, &csp->name);
	csp->qName = cloneString(chain->qName);
	csp->tName = cloneString(chain->tName);
	csp->qStrand = chain->qStrand;
	}
    slAddHead(&csp->chain, chain);
    count++;
    }
lineFileClose(&chainsLf);
printf("read in %d chains\n",count);

for(csp = cspList; csp; csp = csp->next)
    {
    slSort(&csp->chain, chainCmpTarget);
//    csp->chain = aggregateChains(csp->chain);
    }

addedBases = deletedBases = 0;
for(sp = spList; sp; sp = sp->next)
    {
#ifdef NOTNOW
    /* find the chains associated with this strand */
    if ((csp = hashFindVal(chainHash, sp->name)) != NULL)
	{
	/* first check to see if psl blocks are in any chains */
	checkInChains(&sp->psl, &csp->chain, outFound, &addedBases);

	/* now extend chains to the right */
	checkAfterChains(&sp->psl, &csp->chain, outFound, &addedBases);

	/* now extend chains to the left */
	slReverse(&sp->psl);
	checkBeforeChains(&sp->psl, &csp->chain, outFound, &addedBases);
	}
#endif

    /* do we still have psl's */
    if (sp->psl != NULL)
	{
	/* make sure we have a chainList */
	chainList = NULL;
	if (csp == NULL)
	    {
	    AllocVar(csp);
	    slAddHead(&cspList, csp);
	    csp->qName = cloneString(sp->psl->qName);
	    csp->tName = cloneString(sp->psl->tName);
	    csp->qStrand = sp->psl->strand[0];
	    dyStringClear(dy);
	    dyStringPrintf(dy, "%s%c%s", csp->qName, csp->qStrand, csp->tName);
	    hashAddSaveName(chainHash, dy->string, csp, &csp->name);
	    }
	for(psl = sp->psl; psl ;  psl = nextPsl)
	    {
	    /* this psl will either fit a chain or make a new one */

	    nextPsl = psl->next;
	    sp->psl  = nextPsl;

	    psl->next = NULL;
	    fakePslList = psl;
	    if (chainList)
		checkInChains(&fakePslList, &chainList, outFound, &addedBases);
	    if (fakePslList == NULL)
		{
		//freez(&psl);
		continue;
		}
	//    if (chainList)
	//	checkAfterChains(&fakePslList, &chainList, outFound, &addedBases);
	    if (fakePslList == NULL)
		{
		//freez(&psl);
		continue;
		}
	    if (chainList)
		checkBeforeChains(&fakePslList, &chainList, outFound, &addedBases);
	    if (fakePslList == NULL)
		{
		//freez(&psl);
		continue;
		}

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

	    pslTabOut(psl, outFound);
	    freez(&psl);
	    }

	csp->chain = slCat(csp->chain, chainList);
	}

    slSort(&csp->chain, chainCmpTarget);
    csp->chain = aggregateChains(csp->chain);
    }
fclose(outFound);
printf("deleted %d bases\n",deletedBases);
printf("added %d bases\n",addedBases);
count = 0;
for(sp = spList; sp; sp = sp->next)
    for(psl = sp->psl ; psl ; psl = psl->next)
	{
	pslTabOut(psl, outNotFound);
	count++;
	}
fclose(outNotFound);

printf("%d psls remain\n",count);

for(csp = cspList; csp; csp = csp->next)
    for(chain = csp->chain ; chain ; chain = chain->next)
	{
	//slSort(&chain->blockList, boxInCmpBoth);
	chain->tStart = chain->blockList->tStart;
	chain->qStart = chain->blockList->qStart;

	for(block = chain->blockList; block; block = block->next)
	    {
	    chain->tEnd = block->tEnd;
	    chain->qEnd = block->qEnd;
	    }
	chainWrite(chain,  outChains);
	}
fclose(outChains);

dyStringFree(&dy);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
maxGap = optionInt("maxGap", maxGap);
chainStitch(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
