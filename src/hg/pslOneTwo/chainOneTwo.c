/* chainOneTwo - keep track of two highest scores. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chain.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainOneTwo - keep track of two highest scores\n"
  "usage:\n"
  "   chainOneTwo in.chain out.chain\n"
  "options:\n"
  "   -clip     clip output based on score of top alignment and score of the second\n"
  "   -clipRatio=ratio  output top score if (S2/S1) > ratio\n"
  );
}

static struct optionSpec options[] = {
   {"clip", OPTION_BOOLEAN},
   {"clipRatio", OPTION_FLOAT},
   {NULL, 0}
};

struct scoreNode
{
    struct scoreNode *next;
    int score1, score2;
    struct chain *one;
    struct chain *two;
};

void chainOneTwo(char *inName, char *outName)
/* chainOneTwo - keep track of two highest scores. */
{
struct hash *chainHash = newHash(0);
struct lineFile *chainLf = lineFileOpen(inName,TRUE);
int scoreNew;
//int score1, score2;
struct scoreNode *node, *nodeList = NULL;
FILE *outf = mustOpen(outName, "w");
struct chain *chain;
boolean doClip = optionExists("clip");
float clipRatio = optionFloat("clipRatio", 0.0);

while ((chain = chainRead(chainLf)) != NULL)
    {
    if ( (node = (struct scoreNode *)hashFindVal(chainHash, chain->qName)) == NULL)
	{
	AllocVar(node);
	slAddHead(&nodeList, node);
	node->score1 = chain->score;
	node->one = chain;
	hashAdd(chainHash, chain->qName, node);
	}
    else if (node->two == NULL)
	{
	node->two = chain;
	node->score2 = chain->score;
	}
    else
	{
	scoreNew = chain->score;

	if (scoreNew > node->score1)
	    {
	    node->two = node->one;
	    node->score2 = node->score1;
	    node->one = chain;
	    node->score1 = scoreNew;
	    }
	else if (scoreNew > node->score2)
	    {
	    node->two = chain;
	    node->score2 = scoreNew;
	    }
	else
	    chainFree(&chain);
	}
    }

lineFileClose(&chainLf);

for(node = nodeList; node; node = node->next)
    {
    if (doClip)
	{
	if ((node->two == NULL) || (clipRatio > (double)node->score2/node->score1))
	    chainWrite(node->one, outf);
	}
    else
	{
	chainWrite(node->one, outf);
	if (node->two)
	    chainWrite(node->two, outf);
	}
    }

carefulClose(&outf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chainOneTwo(argv[1], argv[2]);
return 0;
}
