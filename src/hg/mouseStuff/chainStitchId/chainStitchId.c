/* chainStitchId - Join chain fragments with the same chain ID into a single chain per ID. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "chain.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainStitchId - Join chain fragments with the same chain ID into a single\n"
  "   chain per ID.  Chain fragments must be from same original chain but\n"
  "   must not overlap.  Chain fragment scores are summed.\n"
  "usage:\n"
  "   chainStitchId in.chain out.chain\n"
/*
  "options:\n"
  "   -xxx=XXX\n"
*/
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void tackOnFrag(struct chain *chain, struct chain *frag)
/* Make sure chain and frag belong together; then expand chain's range 
 * to encompass frag's range and give frag's blockList and score to chain. */
{
if (chain->id != frag->id)
    errAbort("tackOnFrag: chain->id (%d) must be equal to frag->id (%d)",
	     chain->id, frag->id);
if (!sameString(chain->tName, frag->tName))
    errAbort("Inconsistent tName for chain id %d: %s vs. %s",
	     chain->id, chain->tName, frag->tName);
if (!sameString(chain->qName, frag->qName))
    errAbort("Inconsistent qName for chain id %d: %s vs. %s",
	     chain->id, chain->qName, frag->qName);
if (chain->qStrand != frag->qStrand)
    errAbort("Inconsistent qStrand for chain id %d: %c vs. %c",
	     chain->id, chain->qStrand, frag->qStrand);

if (frag->tStart < chain->tStart)
    chain->tStart = frag->tStart;
if (frag->tEnd > chain->tEnd)
	chain->tEnd = frag->tEnd;
if (frag->qStart < chain->qStart)
    chain->qStart = frag->qStart;
if (frag->qEnd > chain->qEnd)
    chain->qEnd = frag->qEnd;

chain->blockList = slCat(chain->blockList, frag->blockList);
frag->blockList = NULL;
chain->score += frag->score;
}

void chainStitchId(char *inChain, char *outChain)
/* chainStitchId - Join chain fragments with the same chain ID into a single chain per ID. */
{
struct lineFile *lf = lineFileOpen(inChain, TRUE);
struct chain *chain = NULL, *chainList = NULL;
FILE *f = mustOpen(outChain, "w");
int idArrLen = 64 * 1024 * 1024;
struct chain **idArr = needLargeZeroedMem(idArrLen * sizeof(struct chain *));
int i=0;

/* Build up an array of chains, indexed by IDs.  Agglomerate chains with same 
 * ID as we go. */
while ((chain = chainRead(lf)) != NULL)
    {
    while (chain->id >= idArrLen)
	{
	idArr = needMoreMem(idArr, idArrLen, idArrLen*2*sizeof(idArr[0]));
	idArrLen *= 2;
	}
    if (idArr[chain->id] == NULL)
	idArr[chain->id] = chain;
    else
	{
	tackOnFrag(idArr[chain->id], chain);
	chainFree(&chain);
	}
    }
lineFileClose(&lf);

/* Clean up each agglomerated chain and add to head of list (but step 
 * backwards so the resulting list is in order by chain id). */
for (i = idArrLen-1;  i >= 0;  i--)
    {
    chain = idArr[i];
    if (chain != NULL)
	{
	slSort(&(chain->blockList), cBlockCmpTarget);
	slAddHead(&chainList, chain);
	}
    }

/* Ordering by original chain id gets us most of the way to sorting by 
 * score, but not all the way: sort and finally write out the chains. */
slSort(&chainList, chainCmpScore);
for (chain = chainList;  chain != NULL;  chain = chain->next)
    {
    chainWrite(chain, f);
    /* could free here, but program is about to end so why waste the time. */
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chainStitchId(argv[1], argv[2]);
return 0;
}
