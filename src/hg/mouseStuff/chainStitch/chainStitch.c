/* chainStitch - Stitch psls into chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "chain.h"
#include "dystring.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainStitch - Stitch psls into chains\n"
  "usage:\n"
  "   chainStitch psls chains outChains outFound outNotFound\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
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

int boxInCmpBoth(const void *va, const void *vb)
/* Compare to sort based on query, then target. */
{
const struct boxIn *a = *((struct boxIn **)va);
const struct boxIn *b = *((struct boxIn **)vb);
int dif;
dif = a->qStart - b->qStart;
if (dif == 0)
    dif = a->tStart - b->tStart;
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
struct boxIn *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;

prevChainBlock = NULL;
if ((te < chain->tStart) && (qe < chain->qStart))
    return FALSE;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if ((chainBlock->tEnd > ts) || (chainBlock->qEnd > qs))
	{
	if (prevChainBlock && !((prevChainBlock->tStart <= ts) && (prevChainBlock->qStart <= qs)))
	    errAbort("bad del\n");
	/*
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
	    */

	return TRUE;
	}
    }
    return FALSE;
}
boolean deleteChainRange(struct chain *chain, int qs, int qe, int ts, int te, int *deletedBases)
{
struct boxIn *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;

prevChainBlock = NULL;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if ((chainBlock->tEnd > ts) || (chainBlock->qEnd > qs))
	{
	if (prevChainBlock && !((prevChainBlock->tStart <= ts) && (prevChainBlock->qStart <= qs)))
	    errAbort("bad del\n");
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

void addPslToChain(struct chain *chain, struct psl *psl, int *addedBases)
{
struct boxIn *bList;
struct boxIn *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;
int qStart = psl->qStart;
int qEnd = psl->qEnd;

if (psl->strand[0] == '-')
    {
    qStart = psl->qSize - psl->qEnd;
    qEnd = psl->qSize - psl->qStart;
    }
prevChainBlock = NULL;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if ((chainBlock->tEnd > psl->tStart) || (chainBlock->qEnd > qStart))
	{
	if  (prevChainBlock && !((prevChainBlock->tEnd <= psl->tStart) && (prevChainBlock->qEnd <= qStart)))
	    errAbort("badd add\n");

	break;
	}
    }

    bList = NULL;
    addPslBlocks(&bList, psl);

    if (prevChainBlock == NULL)
	{
	chain->blockList = bList;
	}
    else
	prevChainBlock->next = bList;

    for(;bList->next; bList = bList->next)
	*addedBases += bList->tEnd - bList->tStart;
    *addedBases += bList->tEnd - bList->tStart;
    bList->next = chainBlock;
}

void chainStitch(char *psls, char *chains, char *outChainName, char *outFoundName, char *outNotFoundName)
/* chainStitch - Stitch psls into chains. */
{
int jj;
struct chain *nextChain, *prevChain;
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
struct boxIn *block , *nextBlock = NULL, *prevBlock = NULL;
struct psl *prevPsl, *nextPsl;
int count;

while ((psl = pslNext(pslLf)) != NULL)
    {
    assert(psl->strand[1] == '+');
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

count = 0;
for(sp = spList; sp; sp = sp->next)
    for(psl = sp->psl ; psl ; psl = psl->next)
	count++;
printf("added %d psls\n",count);
while ((chain = chainRead(chainsLf)) != NULL)
    {
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
    }
lineFileClose(&chainsLf);

/* for all strands that have psl blocks */
count = 0;
for(csp = cspList; csp; csp = csp->next)
    for(chain = csp->chain ; chain ; chain = chain->next)
	count++;
printf("added %d chains\n",count);

addedBases = deletedBases = 0;
for(sp = spList; sp; sp = sp->next)
    {
    //printf("checking %s\n",sp->name);
    /* find the chains associated with this strand */
    if ((csp = hashFindVal(chainHash, sp->name)) == NULL)
	{
	/* report these blocks as not found */
	continue;
	}

    for(jj = 0; jj < 2; jj++)
    {
    int fudge = jj * 100000;
    prevPsl = NULL;
    for(psl = sp->psl; psl ;  psl = nextPsl)
	{
	nextPsl = psl->next;
	int qStart = psl->qStart;
	int qEnd = psl->qEnd;

	if (psl->strand[0] == '-')
	    {
	    qStart = psl->qSize - psl->qEnd;
	    qEnd = psl->qSize - psl->qStart;
	    }

	prevChain = 0;
	for(chain = csp->chain ; chain ; prevChain = chain , chain = nextChain)
	    {
	    nextChain = chain->next;
	    if (((psl->tStart <= chain->tEnd + fudge) && (psl->tEnd > chain->tStart - fudge)) &&
	         (qStart <= chain->qEnd + fudge) && (qEnd > chain->qStart - fudge))
		{
		//if (deleteChainRange(chain, qStart, qEnd, psl->tStart, psl->tEnd, &deletedBases))
		if (fudge && !checkChainRange(chain, qStart, qEnd, psl->tStart, psl->tEnd))
		    addPslToChain(chain, psl, &addedBases);
		if (chain->blockList == NULL)
		    {
		    if (prevChain)
			{
			prevChain->next = nextChain;
			}
		    else
			{
			csp->chain = nextChain;
			}
		    freez(&chain);
		    }
		if (prevPsl != NULL)
		    prevPsl->next = nextPsl;
		else
		    sp->psl = nextPsl;
		    
		pslTabOut(psl, outFound);
		freez(&psl);
		break;
		}
	    }
	if (chain == NULL)
	    prevPsl = psl;
	
	}
	}
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
chainStitch(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
