/* chainOverlap - Stitch chains into chains. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"
#include "chain.h"
#include "dystring.h"

boolean onQuery = FALSE;
 
void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainOverlap - score chains by average of overlap per base\n"
  "usage:\n"
  "   chainOverlap chainFile\n"
  "options:\n"
  "    -query   check overlap on query instead of target\n"
  );
}

static struct optionSpec options[] = {
   {"query", OPTION_BOOLEAN},
   {NULL, 0},
};

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *tName;		/* Name of target sequence. */
    unsigned tSize;
    struct chain *chainList;
    };


void chainOverlap(char *chainName)
/* chainOverlap - Stitch chains into chains. */
{
struct lineFile *lf = lineFileOpen(chainName, TRUE);
int lastChainId = -1;
struct seqPair *spList = NULL, *sp;
struct dyString *dy = newDyString(512);
struct hash *chainHash = newHash(0);  
struct chain *chain, *chainList = NULL;
int count;

count = 0;
while ((chain = chainRead(lf)) != NULL)
    {
    count++;
    sp = hashFindVal(chainHash, chain->tName);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	sp->tName = cloneString(chain->tName);
	hashAdd(chainHash, sp->tName, sp);
	if (onQuery)
	    sp->tSize = chain->qSize;
	else
	    sp->tSize = chain->tSize;
	}

    slAddHead(&sp->chainList, chain);
    }
lineFileClose(&lf);
verbose(2,"read in  %d chains\n",count);

for(sp = spList; sp; sp = sp->next)
    {
    unsigned short *mem = NULL;

    slSort(&sp->chainList,chainCmpTarget);

    mem = needLargeMem(sp->tSize * sizeof(*mem));

    for(chain = sp->chainList; chain ;  chain = chain->next)
	{
	struct cBlock *cb = chain->blockList;

	for(cb = chain->blockList; cb ; cb = cb->next)
	    {
	    int ii;
	    int start = cb->tStart;
	    int end = cb->tEnd;

	    if (onQuery)
		{
		start = cb->qStart;
		end = cb->qEnd;
		}

	    for(ii=start; ii < end; ii++)
		if ( mem[ii] < 65535)
		    mem[ii]++;
	    }
	}

    for(chain = sp->chainList; chain ;  chain = chain->next)
	{
	struct cBlock *cb = chain->blockList;
	int score = 0;
	int numBases = 0;

	for(cb = chain->blockList; cb ; cb = cb->next)
	    {
	    int ii;
	    int start, end;

	    if (onQuery)
		{
		numBases += cb->qEnd - cb->qStart;
		start = cb->qStart;
		end = cb->qEnd;
		}
	    else
		{
		numBases += cb->tEnd - cb->tStart;
		start = cb->tStart;
		end = cb->tEnd;
		}

	    for(ii=start; ii < end; ii++)
		score += mem[ii];
	    }

	chain->score = 100 * (score / (double) numBases) ;
	chainWrite(chain, stdout);
	}
	
    freez(&mem);
    }


dyStringFree(&dy);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
chainOverlap(argv[1]);
return 0;
}
