/* newStitch3 - Another stitching experiment - with kd-trees.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "axt.h"
#include "portable.h"
#include "chainBlock.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "newStitch4 - Another stitching experiment - with kd-trees.\n"
  "usage:\n"
  "   newStitch4 in.axt out.s4\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    struct boxIn *blockList; /* List of alignments. */
    };

int defaultGapPenalty(int qSize, int tSize)
/* A logarithmic gap penalty scaled to axt defaults. */
{
int total = qSize + tSize;
if (total <= 0)
    return 0;
return 400 * pow(total, 1.0/2.5);
}

int gapCost(struct boxIn *a, struct boxIn *b)
/* Return gap score from a to b */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
if (dq < 0) dq = 0;
if (dt < 0) dt = 0;
return defaultGapPenalty(dq, dt);
}

void chainPair(struct seqPair *sp, FILE *f)
/* Make chains for all alignments in sp. */
{
long startTime, dt;
struct axt *axt;
struct boxIn *blockList, *block;
struct chain *chainList = NULL, *chain;

uglyf("%s %d nodes\n", sp->name, slCount(sp->blockList));

/* Make up tree and time it for debugging. */
startTime = clock1000();
chainList = chainBlocks(&sp->blockList, gapCost);
dt = clock1000() - startTime;
uglyf("Made %d chains in %5.3f seconds\n", slCount(chainList), dt*0.001);

/* Dump chains to file. */
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct boxIn *first = chain->blockList;
    struct boxIn *last = slLastEl(first);
    struct boxIn *block;
    fprintf(f, "%s Chain %d, score %d, %d %d, %d %d:\n", 
	sp->name, slCount(chain->blockList), chain->score,
	first->qStart, last->qEnd, first->tStart, last->qEnd);
    for (block = chain->blockList; block != NULL; block = block->next)
        {
	fprintf(f, " %s q %d, t %d, score %d\n", sp->name,
		block->qStart, block->tStart, block->score);
	}
    fprintf(f, "\n");
    }
chainFreeList(&chainList);
uglyf("\n");
}


void newStitch3(char *axtFile, char *output)
/* newStitch3 - Another stitching experiment - with kd-trees.. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(output, "w");

/* Read input file and divide alignments into various parts. */
while ((axt = axtRead(lf)) != NULL)
    {
    struct boxIn *block;
    if (axt->score < 500)
        {
	axtFree(&axt);
	continue;
	}
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	}
    AllocVar(block);
    block->qStart = axt->qStart;
    block->qEnd = axt->qEnd;
    block->tStart = axt->tStart;
    block->tEnd = axt->tEnd;
    block->score = axt->score;
    slAddHead(&sp->blockList, block);
    axtFree(&axt);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->blockList);
    chainPair(sp, f);
    }
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
newStitch3(argv[1], argv[2]);
return 0;
}
