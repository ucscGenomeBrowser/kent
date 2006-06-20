/* chainStats - Stitch psls into chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "chain.h"
#include "dystring.h"

int fudge=5000;
 
void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainStats - Stitch psls into chains\n"
  "usage:\n"
  "   chainStats chains \n"
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




int gapChains( struct chain *chainIn)
{
int count = 0;
struct chain *chain1=NULL, *chain2, *chain2Next, *prevChain2;

//printf("%s%c%s\n",chainIn->qName, chainIn->qStrand, chainIn->tName);
while(chainIn)
    {
    //if (chain1 != chainIn)
    //printf("chainIn %x\n",chainIn);
    chain1 = chainIn;
    //slSort(&chainIn, chainCmpTarget);
    prevChain2 = NULL;
    for(chain2 = chainIn->next; chain2 ;  chain2 = chain2Next)
	{
	    chain2Next = chain2->next;
	    if ((chain1->tStart <= chain2->tStart) && (chain1->tEnd + fudge > chain2->tStart)
	       && (chain1->qStart <= chain2->qStart) && (chain1->qEnd + fudge > chain2->qStart) 
	 &&	(chain1->qEnd <= chain2->qStart) && (chain1->tEnd <= chain2->tStart))
		{
		count++;
		assert(chain1->tStart < chain1->tEnd);
		assert(chain1->qStart < chain1->qEnd);
		assert(chain2->tStart < chain2->tEnd);
		assert(chain2->qStart < chain2->qEnd);
		assert(chain1->qEnd <= chain2->qStart);
		assert(chain1->tEnd <= chain2->tStart);
		printf("connect at t %s:%d-%d q %s:%d-%d    ",chain1->tName, chain1->tStart, chain1->tEnd, chain1->qName, chain1->qStart, chain1->qEnd);
		printf("t %s:%d-%d q %s:%d-%d\n",chain2->tName, chain2->tStart, chain2->tEnd, chain2->qName, chain2->qStart, chain2->qEnd);
		//chain1->tStart = chain1->tStart;
		//chain1->qStart = chain1->qStart;
		//chain1->tEnd = max(chain1->tEnd, chain2->tEnd);
		//chain1->qEnd = max(chain1->qEnd, chain2->qEnd);
		chain2 = NULL;
		/*
		chain1->tEnd =  chain2->tEnd;
		chain1->qEnd =  chain2->qEnd;
		chain1->blockList = slCat(chain1->blockList,chain2->blockList);
		chain2->blockList = NULL;
		if (prevChain2)
		    prevChain2->next = chain2Next;
		else
		    chainIn->next = chain2Next;
		chain2 = NULL;
	//	slSort(&chain2->blockList,  boxInCmpBoth);
		//freez(&chain2);
		*/
		}
	    else
		prevChain2 = chain2;
	    /*
	    else if ((chain1->tStart >= chain2->tEnd) && (chain1->tStart - fudge < chain2->tEnd)
		&& (chain1->qStart >= chain2->qEnd) && (chain1->qStart - fudge < chain2->qEnd))
		{
		errAbort("shouldn't get here\n");
		chain2->tEnd = chain1->tEnd;
		chain2->qEnd = chain1->qEnd;
		chain2->blockList = slCat(chain2->blockList,chain1->blockList);
		freez(&chain1);
		break;
		}
		*/
	}


    //if (chain2 == NULL)
	{
	chainIn = chainIn->next;
//	slAddHead(&outChain, chain1);
	}
    }
return count;
}

void chainStats(char *chains)
{
int lastChainId = -1;
struct lineFile *chainsLf = lineFileOpen(chains, TRUE);
struct cseqPair *cspList = NULL, *csp;
struct dyString *dy = newDyString(512);
struct hash *chainHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct chain *chain;
struct cBlock *block;
int count;

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
    gapChains(csp->chain);
    for(chain = csp->chain ; chain ; chain = chain->next)
	{
	for(block = chain->blockList; block; block = block->next)
	    {
	    }
	}
    }

dyStringFree(&dy);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
chainStats(argv[1]);
return 0;
}
