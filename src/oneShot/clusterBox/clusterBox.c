/* clusterBox - Cluster together overlapping boxes,  or technically,
 * overlapping axt's. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "dlist.h"
#include "portable.h"
#include "options.h"
#include "localmem.h"
#include "dlist.h"
#include "fuzzyFind.h"
#include "axt.h"
#include "boxClump.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterBox - Cluster together overlaping boxes, or, technically,\n"
  "overlapping axt format alignments\n"
  "usage:\n"
  "   clusterBox input.axt output\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void printClumps(struct boxClump *clumpList, FILE *f)
/* Print out all clumps. */
{
struct boxClump *clump;
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    struct boxIn *box;
    fprintf(f, "Cluster of %d %d,%d %d,%d\n", clump->boxCount,
	clump->qStart, clump->qEnd, clump->tStart, clump->tEnd);
    for (box = clump->boxList; box != NULL; box = box->next)
        fprintf(f, "  %d,%d %d,%d\n", 
		box->qStart, box->qEnd, box->tStart, box->tEnd);
    }
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	/* Allocated in hash */
    struct axt *axtList; /* List of alignments. */
    };

void clusterPair(struct seqPair *sp, FILE *f)
/* Make chains for all alignments in sp. */
{
struct boxIn *boxList = NULL, *box;
struct boxClump *clumpList, *clump;
struct axt *axt;

/* Make initial list containing all axt's. */
fprintf(f, "Pairing %s - %d axt's initially\n", sp->name, slCount(sp->axtList));

/* Make a box for each axt. */
for (axt = sp->axtList; axt != NULL; axt = axt->next)
    {
    AllocVar(box);
    box->qStart = axt->qStart;
    box->qEnd = axt->qEnd;
    box->tStart = axt->tStart;
    box->tEnd = axt->tEnd;
    box->data = axt;
    slAddHead(&boxList, box);
    }

/* Cluster and print. */
clumpList = boxFindClumps(&boxList);
slSort(&clumpList, boxClumpCmpCount);
printClumps(clumpList, f);
fprintf(f, "\n");
}


void clusterAxt(char *axtFile, char *output)
/* clusterAxt - Cluster together overlapping boxes,  or technically,
 * overlapping axt's. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(output, "w");
int axtCount = 0;
long t1,t2,t3;

/* Read input file and divide alignments into various parts. */
t1 = clock1000();
while ((axt = axtRead(lf)) != NULL)
    {
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
    slAddHead(&sp->axtList, axt);
    ++axtCount;
    }
uglyf("%d axt's\n", axtCount);
t2 = clock1000();
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->axtList);
    clusterPair(sp, f);
    }
t3 = clock1000();
dyStringFree(&dy);
uglyf("axtRead %fs, clusterPair %fs\n", 0.001 * (t2-t1), 0.001 * (t3-t2) );
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
clusterAxt(argv[1], argv[2]);
return 0;
}
