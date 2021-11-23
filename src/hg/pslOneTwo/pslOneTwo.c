/* pslOneTwo - keep track of two highest scores. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslOneTwo - keep track of two highest scores\n"
  "usage:\n"
  "   pslOneTwo in.psl out.psl\n"
  "options:\n"
  "   -clip     clip output based on score of top alignment and score of the second\n"
  "   -clipRatio=ratio  output top score if (S2/S1) < ratio. Default 0\n"
  );
}

static struct optionSpec options[] = {
   {"clip", OPTION_BOOLEAN},
   {"clipRatio", OPTION_FLOAT},
   {NULL,0}
};

struct scoreNode
{
    struct scoreNode *next;
    int score1, score2;
    struct psl *one;
    struct psl *two;
};

void pslOneTwo(char *inName, char *outName)
/* pslOneTwo - keep track of two highest scores. */
{
struct hash *pslHash = newHash(0);
struct lineFile *pslLf = pslFileOpen(inName);
int scoreNew;
struct scoreNode *node, *nodeList = NULL;
FILE *outf = mustOpen(outName, "w");
struct psl *psl;
boolean doClip = optionExists("clip");
float clipRatio = optionFloat("clipRatio", 0.0);

while ((psl = pslNext(pslLf)) != NULL)
    {
    if ( (node = (struct scoreNode *)hashFindVal(pslHash, psl->qName)) == NULL)
	{
	AllocVar(node);
	slAddHead(&nodeList, node);
	node->score1 = pslScore(psl);
	node->one = psl;
	hashAdd(pslHash, psl->qName, node);
	}
    else
	{
	scoreNew = pslScore(psl);

	if (scoreNew > node->score1)
	    {
	    node->two = node->one;
	    node->score2 = node->score1;
	    node->one = psl;
	    node->score1 = scoreNew;
	    }
	else if (scoreNew > node->score2)
	    {
	    node->two = psl;
	    node->score2 = scoreNew;
	    }
	else
	    pslFree(&psl);
	}
    }

lineFileClose(&pslLf);

for(node = nodeList; node; node = node->next)
    {
    if (doClip)
	{
        struct psl *p = node->one;
        verbose(2,"ratio %f < %f scores %d %d %s\n",
                (double)node->score2/node->score1, clipRatio, node->score1, node->score2, p->qName);
	if ((node->two == NULL) || (clipRatio > (double)node->score2/node->score1))
	    pslTabOut(node->one, outf);
	}
    else
	{
	pslTabOut(node->one, outf);
	if (node->two)
	    pslTabOut(node->two, outf);
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
pslOneTwo(argv[1], argv[2]);
return 0;
}
