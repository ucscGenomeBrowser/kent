/* simpleChain - Stitch psls into chains. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "chain.h"
#include "dystring.h"

int maxGap=250000;
boolean outPsl = FALSE;
int sizeMul = 1;
char *onlyChrom = NULL;
int intToPslSize = 1024 * 1024;
 
void usage()
/* Explain usage and exit. */
{
errAbort(
  "simpleChain - Stitch psls into chains\n"
  "usage:\n"
  "   simpleChain psls outChains \n"
  "options:\n"
  "   -prot        input and output are protein psls\n"
  "   -maxGap=N    defines max gap possible (default 250,000)\n"
  "   -outPsl      output psl format instead of chain\n"
  "   -ignoreQ     ignore query name and link together everything\n"
  "   -chrom=chr2  ignore alignments not on chr2\n"
  "   -hSize=#     size of intToPsl hash table size (default 1024*1024)\n"
  );
}

static struct optionSpec options[] = {
   {"maxGap", OPTION_INT},
   {"prot", OPTION_BOOLEAN},
   {"outPsl", OPTION_BOOLEAN},
   {"ignoreQ", OPTION_BOOLEAN},
   {"chrom", OPTION_STRING},
   {"hSize", OPTION_INT},
   {NULL, 0},
};

struct intToPslStruct
{
struct intToPslStruct *next;
int val;
struct psl *psl;
};

struct intToPslStruct **intToPslArray;

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    unsigned tSize, qSize;
    struct psl *psl;
    };


int storePsl(struct psl *psl)
{
static int numId = 0;
struct intToPslStruct *ptr;
int ret = numId;

AllocVar(ptr);
ptr->psl = psl;
ptr->val = numId;
ptr->next = intToPslArray[numId % intToPslSize];
intToPslArray[numId % intToPslSize] = ptr;

numId++;

return ret;
}

struct psl *getPsl(int val)
{
struct intToPslStruct *ptr;

ptr = intToPslArray[val % intToPslSize];
for(; ptr && (ptr->val != val); ptr = ptr->next)
    ;

if (ptr)
    {
    assert(ptr->val == val);
    assert(ptr->psl != NULL);
    return ptr->psl;
    }

errAbort("Can't find psl");
return NULL;
}

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
    b->tEnd += sizeMul *size;
    slAddHead(pList, b);
    }
slReverse(pList);
}

boolean addPslToChain(struct chain *chain, struct psl *psl, int *addedBases)
{
struct cBlock *bList;
struct cBlock *chainBlock , *nextChainBlock = NULL, *prevChainBlock = NULL;
int qStart = psl->qStarts[0];
int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
int tStart = psl->tStarts[0];
int tEnd = psl->tStarts[psl->blockCount - 1] + sizeMul * psl->blockSizes[psl->blockCount - 1];
struct psl *totalPsl = getPsl(chain->id);

prevChainBlock = NULL;
for(chainBlock = chain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = nextChainBlock)
    {
    nextChainBlock = chainBlock->next;
    if ((chainBlock->tEnd > tStart) || (chainBlock->qEnd > qStart))
	{
	if  (prevChainBlock && !((prevChainBlock->tEnd <= tStart) && (prevChainBlock->qEnd <= qStart)))
	    return FALSE;
	if  (chainBlock && !((chainBlock->tStart >= tEnd) && (chainBlock->qStart >= qEnd)))
	    return FALSE;

	break;
	}
    }

    if (outPsl)
	{
	totalPsl->match += psl->match;
	totalPsl->misMatch += psl->misMatch;
	totalPsl->repMatch += psl->repMatch;
	totalPsl->nCount += psl->nCount;	
	totalPsl->qNumInsert += psl->qNumInsert;
	totalPsl->qBaseInsert += psl->qBaseInsert;
	totalPsl->tNumInsert += psl->tNumInsert;
	totalPsl->tBaseInsert += psl->tBaseInsert;
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
struct psl *totalPsl1;
struct psl *totalPsl2;

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

		if (outPsl)
		    {
		    totalPsl1 = getPsl(chain1->id);
		    totalPsl2 = getPsl(chain2->id);
		    totalPsl1->match += totalPsl2->match;
		    totalPsl1->misMatch += totalPsl2->misMatch;
		    totalPsl1->repMatch += totalPsl2->repMatch;
		    totalPsl1->nCount += totalPsl2->nCount;	
		    totalPsl1->qNumInsert += totalPsl2->qNumInsert;
		    totalPsl1->qBaseInsert += totalPsl2->qBaseInsert;
		    totalPsl1->tNumInsert += totalPsl2->tNumInsert;
		    totalPsl1->tBaseInsert += totalPsl2->tBaseInsert;
		    }
	
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

void checkInChains(struct psl **pslList, struct chain **chainList, FILE *outFound, int *addedBases)
{
struct chain *nextChain;
struct chain *chain;
struct psl *psl;
struct psl *prevPsl, *nextPsl;

prevPsl = NULL;
for(psl = *pslList; psl ;  psl = nextPsl)
    {
    int qStart = psl->qStarts[0];
    int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
    int tStart = psl->tStarts[0];
    int tEnd = psl->tStarts[psl->blockCount - 1] + sizeMul * psl->blockSizes[psl->blockCount - 1];
    nextPsl = psl->next;

    assert(tEnd > tStart);
    assert(qEnd > qStart);
    for(chain = *chainList; chain ; chain = nextChain)
	{
	nextChain = chain->next;
	if (((tStart < chain->tEnd) && (tEnd > chain->tStart)) &&
	     (qStart < chain->qEnd) && (qEnd > chain->qStart))
	    {
	    if ( addPslToChain(chain, psl, addedBases))
		{
		//pslTabOut(psl, outFound);
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

void chainToPslWrite(struct chain *inChain, FILE *outFile, int tSize, int qSize)
/* write out a chain in psl format */
{
struct psl *totalPsl = getPsl(inChain->id);
int lastTEnd=0, lastQEnd=0;
struct psl psl;
struct cBlock *chainBlock, *prevChainBlock = NULL;
int qInsert, tInsert;
int blockNum;
static unsigned *blockSizes = NULL;
static unsigned *qStarts = NULL;
static unsigned *tStarts = NULL;
static int maxCount = 0;

memset(&psl, 0, sizeof(psl));

psl.strand[0] = inChain->qStrand;
psl.qName = inChain->qName;
psl.tName = inChain->tName;
psl.tSize = tSize;
psl.qSize = qSize;
psl.tStart = inChain->tStart;
psl.tEnd = inChain->tEnd;


if (inChain->qStrand == '-')
    {
    psl.qStart = qSize - inChain->qEnd;
    psl.qEnd = qSize - inChain->qStart;
    }
else
    {
    psl.qStart = inChain->qStart;
    psl.qEnd = inChain->qEnd;
    }

for(chainBlock = inChain->blockList; chainBlock; chainBlock = chainBlock->next)
    psl.blockCount++;

if (psl.blockCount > maxCount)
    {
    maxCount = psl.blockCount + 100;
    blockSizes = realloc(blockSizes, maxCount * sizeof(int));
    qStarts = realloc(qStarts, maxCount * sizeof(int));
    tStarts = realloc(tStarts, maxCount * sizeof(int));
    }
psl.blockSizes = blockSizes;
psl.qStarts = qStarts;
psl.tStarts = tStarts;


prevChainBlock = NULL;
blockNum = 0;
for(chainBlock = inChain->blockList; chainBlock; prevChainBlock = chainBlock, chainBlock = chainBlock->next)
    {
    if (prevChainBlock != NULL)
	{
	tInsert = chainBlock->tStart - lastTEnd;
	if (tInsert)
	    {
	    psl.tNumInsert++;
	    psl.tBaseInsert += tInsert;
	    psl.misMatch += tInsert;
	    }

	qInsert = chainBlock->qStart - lastQEnd;
	if (qInsert)
	    {
	    psl.qNumInsert++;
	    psl.qBaseInsert += qInsert;
	    }
	}

    lastTEnd = chainBlock->tEnd;
    lastQEnd = chainBlock->qEnd;
    blockSizes[blockNum] = chainBlock->qEnd - chainBlock->qStart;
    psl.match += blockSizes[blockNum];
    qStarts[blockNum] = chainBlock->qStart;
    tStarts[blockNum] = chainBlock->tStart;
    blockNum++;
    }

psl.match = totalPsl->match;
psl.misMatch = totalPsl->misMatch;
psl.repMatch = totalPsl->repMatch;
psl.nCount = totalPsl->nCount;	
psl.qNumInsert = totalPsl->qNumInsert;
psl.qBaseInsert = totalPsl->qBaseInsert;
psl.tNumInsert = totalPsl->tNumInsert;
psl.tBaseInsert = totalPsl->tBaseInsert;
psl.strand[0] = totalPsl->strand[0];
psl.strand[1] = totalPsl->strand[1];
if (totalPsl->strand[1] == '-')
    {
    psl.tEnd = psl.tSize - psl.tStarts[0];
    psl.tStart = psl.tSize - (psl.tStarts[blockNum - 1] + sizeMul*psl.blockSizes[blockNum - 1]);
    }
else
    {
    psl.tStart = psl.tStarts[0];
    psl.tEnd = psl.tStarts[blockNum - 1] + sizeMul*psl.blockSizes[blockNum - 1];
    }
pslComputeInsertCounts(&psl);
pslTabOut(&psl, outFile);
}

void simpleChain(char *psls,  char *outChainName)
/* simpleChain - Stitch psls into chains. */
{
struct psl *newPsl = 0;
int lastChainId = -1;
struct psl *nextPsl;
struct psl *fakePslList;
int deletedBases, addedBases;
FILE *outChains = mustOpen(outChainName, "w");
struct seqPair *spList = NULL, *sp;
struct lineFile *pslLf = pslFileOpen(psls);
struct dyString *dy = newDyString(512);
struct psl *psl;
struct hash *pslHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct chain *chain, *chainList = NULL;
int count;

count = 0;
while ((psl = pslNext(pslLf)) != NULL)
    {
    //assert((psl->strand[1] == 0) || (psl->strand[1] == '+'));
    if ((onlyChrom != NULL) && (!sameString(onlyChrom, psl->tName)))
	{
	freez(&psl);
	continue;
	}

    count++;
    dyStringClear(dy);
    if ( optionExists("ignoreQ"))
	dyStringPrintf(dy, "%s%s%s", "", psl->strand, psl->tName);
    else
	dyStringPrintf(dy, "%s%s%s", psl->qName, psl->strand, psl->tName);
    sp = hashFindVal(pslHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pslHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(psl->qName);
	sp->tName = cloneString(psl->tName);
	sp->qStrand = psl->strand[0];
	sp->tSize = psl->tSize;
	sp->qSize = psl->qSize;
	}

    slAddHead(&sp->psl, psl);
    }
lineFileClose(&pslLf);
verbose(2,"read in  %d psls\n",count);

addedBases = deletedBases = 0;
for(sp = spList; sp; sp = sp->next)
    {
    int count = 0;
    /* make sure we have a chainList */
    chainList = NULL;
    slSort(&sp->psl,pslCmpScoreDesc);

    for(psl = sp->psl; psl ;  psl = nextPsl)
	{

	int qStart = psl->qStarts[0];
	int qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	int tStart = psl->tStarts[0];
	int tEnd = psl->tStarts[psl->blockCount - 1] + sizeMul * psl->blockSizes[psl->blockCount - 1];
	
	nextPsl = psl->next;
	sp->psl  = nextPsl;

	assert(tEnd > tStart);
	assert(qEnd > qStart);
	/* check for overlap */
	psl->next = 0;
	fakePslList = psl;
	if (chainList)
	    checkInChains(&fakePslList, &chainList, NULL, &addedBases);
	if (fakePslList == NULL)
	    {
	    //freez(&psl);
	    continue;
	    }
	count++;

	AllocVar(chain);
	chain->tStart = psl->tStarts[0];
	chain->qStart = psl->qStarts[0];
	chain->tEnd = psl->tStarts[psl->blockCount - 1] + sizeMul * psl->blockSizes[psl->blockCount - 1];
	chain->qEnd = psl->qStarts[psl->blockCount - 1] + psl->blockSizes[psl->blockCount - 1];
	chain->tSize = psl->tSize;
	chain->qSize = psl->qSize;
	chain->qName = cloneString(psl->qName);
	chain->tName = cloneString(psl->tName);
	chain->qStrand = psl->strand[0];
	if (outPsl)
	    {
	    AllocVar(newPsl);
	    chain->id = storePsl(newPsl);
	    newPsl->strand[0] = psl->strand[0];
	    newPsl->strand[1] = psl->strand[1];
	    newPsl = 0;
	    }
    	else
	    chain->id = ++lastChainId;

	if (!addPslToChain(chain, psl, &addedBases))
	    errAbort("new ");
	slAddHead(&chainList, chain);
	chainList = aggregateChains(chainList);

	}

    for(chain = aggregateChains(chainList); chain ;  chain = chain->next)
	{
	if (outPsl)
	    chainToPslWrite(chain, outChains, sp->tSize, sp->qSize);
	else
	    {
	    //chain->qEnd = (chain->qEnd < chain->qSize)? chain->qEnd : chain->qSize;
	    chainWrite(chain, outChains);
	    }
	}
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
outPsl = optionExists("outPsl");
sizeMul = optionExists("prot") ? 3 : 1;
onlyChrom = optionVal("chrom", NULL);
intToPslSize = optionInt("hSize", intToPslSize);

intToPslArray = needLargeMem(sizeof( struct intToPslStruct *) * intToPslSize);
simpleChain(argv[1], argv[2]);
return 0;
}
