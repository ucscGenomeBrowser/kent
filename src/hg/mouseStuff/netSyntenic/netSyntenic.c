/* netSyntenic - Add synteny info to net. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "chainNet.h"
#include "rbTree.h"


struct lm *lm;
struct rbTreeNode **rbStack;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netSyntenic - Add synteny info to net.\n"
  "usage:\n"
  "   netSyntenic in.net out.net\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Use empty optionSpec so we can use verbose() later. */
struct optionSpec options[] = {
   {NULL, 0},
};

struct chrom 
/* Info on one chromosome */
    {
    struct chrom *next;
    char *name;		         /* Name - allocated in hash */
    struct rbTree *dupeTree;     /* Coverage depth tree. */
    };

struct dupeRange
/* A part of a chromosome. */
    {
    int start, end;	/* Half open zero based coordinates. */
    int depth;		/* Depth of coverage. */
    };

int dupeRangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct dupeRange *a = va;
struct dupeRange *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

void dupeTreeAddSub(struct rbTree *dupeTree, int start, int end, int dir)
/* Add or subtract coverage in given range.  Dir is  + or -1. */
{
struct dupeRange key, *r, *nr;
struct slRef *ref, *refList;
int lastEnd = start;

if (start == end)
    return;
key.start = start;
key.end = end;
refList = rbTreeItemsInRange(dupeTree, &key, &key);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    r = ref->val;
    if (lastEnd != r->start)
        {
	lmAllocVar(dupeTree->lm, nr);
	if (lastEnd < r->start)
	    {
	    nr->start = lastEnd;
	    nr->end = r->start;
	    nr->depth = dir;
	    rbTreeAdd(dupeTree, nr);
	    }
	else
	    {
	    nr->start = r->start;
	    nr->end = lastEnd;
	    nr->depth = r->depth;
	    r->start = lastEnd;
	    rbTreeAdd(dupeTree, nr);
	    }
	}
    if (end < r->end)
        {
	lmAllocVar(dupeTree->lm, nr);
	nr->start = end;
	nr->end = r->end;
	nr->depth = r->depth;
	r->end = end;
	rbTreeAdd(dupeTree, nr);
	}
    r->depth += dir;
    lastEnd = r->end;
    }
if (lastEnd < end)
    {
    lmAllocVar(dupeTree->lm, nr);
    nr->start = lastEnd;
    nr->end = end;
    nr->depth = dir;
    rbTreeAdd(dupeTree, nr);
    }
slFreeList(&refList);
}

int dupeTreeCountOver(struct rbTree *dupeTree, int start, int end, int threshold)
/* Count bases covered by more than threshold between start and end. */
{
struct slRef *refList, *ref;
struct dupeRange key, *r;
int count = 0;

key.start = start;
key.end = end;
refList = rbTreeItemsInRange(dupeTree, &key, &key);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    r = ref->val;
    if (r->depth >= threshold)
        count += rangeIntersection(start, end, r->start, r->end);
    }
slFreeList(&refList);
return count;
}


void dupeTreeAdd(struct rbTree *dupeTree, int start, int end)
/* Subtract coverage in given range. */
{
dupeTreeAddSub(dupeTree, start, end, 1);
}

void dupeTreeSubtract(struct rbTree *dupeTree, int start, int end)
/* Subtract coverage in given range. */
{
dupeTreeAddSub(dupeTree, start, end, -1);
}

#ifdef TEST
void testDupeTree()
/* Test that dupe tree basically works. */
{
struct rbTree *dupeTree = rbTreeNew(dupeRangeCmp);
struct slRef *refList, *ref;
struct dupeRange key;

dupeTreeAdd(dupeTree, 10, 20);
dupeTreeAdd(dupeTree, 30, 40);
dupeTreeAdd(dupeTree, 15, 35);
dupeTreeAdd(dupeTree, 5,  45);
dupeTreeAdd(dupeTree, 35, 50);
dupeTreeAdd(dupeTree, 0, 25);
dupeTreeSubtract(dupeTree, 5, 55);

key.start = -10;
key.end = 60;
refList = rbTreeItemsInRange(dupeTree, &key, &key);
uglyf("Results\n");
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct dupeRange *r = ref->val;
    uglyf(" %d %d depth %d\n", r->start, r->end, r->depth);
    }
uglyf("%d bases covered at least 2\n", dupeTreeCountOver(dupeTree, 0, 60, 2));
uglyAbort("All for now");
}
#endif /* TEST */


void rCalcDupes(struct chainNet *net,  struct hash *qChromHash, 
      struct cnFill *fillList)
/* Recursively add duplicate sequence to tree. */
{
struct cnFill *fill;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    struct chrom *qChrom = hashFindVal(qChromHash, fill->qName);
    struct rbTree *dupeTree;
    int start = fill->qStart;
    int end = start + fill->qSize;
    if (qChrom == NULL)
        {
	AllocVar(qChrom);
	hashAddSaveName(qChromHash, fill->qName, qChrom, &qChrom->name);
	qChrom->dupeTree = rbTreeNewDetailed(dupeRangeCmp, lm, rbStack);
	}
    dupeTree = qChrom->dupeTree;
    if (fill->chainId)
        dupeTreeAdd(dupeTree, start, end);
    else
        dupeTreeSubtract(dupeTree, start, end);
    if (fill->children != NULL)
        rCalcDupes(net, qChromHash, fill->children);
    }
}

void printMem()
/* Print out memory used and other stuff from linux. */
{
struct lineFile *lf = lineFileOpen("/proc/self/stat", TRUE);
char *line, *words[50];
int wordCount;
if (lineFileNext(lf, &line, NULL))
    {
    wordCount = chopLine(line, words);
    if (wordCount >= 23)
        verbose(1, "memory usage %s, utime %s s/100, stime %s\n", 
		words[22], words[13], words[14]);
    }
lineFileClose(&lf);
}

void rNetSyn(struct chainNet *net,  struct hash *qChromHash,
	struct cnFill *fillList, struct cnFill *parent)
/* Recursively classify syntenically. */
{
struct cnFill *fill;
char *type;
for (fill = fillList; fill != NULL; fill = fill->next)
    {
    if (fill->chainId)
        {
	int fStart = fill->qStart;
	int fEnd = fStart + fill->qSize;
	struct chrom *chrom = hashMustFindVal(qChromHash, fill->qName);

	fill->qDup = dupeTreeCountOver(chrom->dupeTree, fStart, fEnd, 2);
	if (parent == NULL)
	    type = "top";
	else if (!sameString(fill->qName, parent->qName))
	    type = "nonSyn";
	else
	    {
	    int pStart = parent->qStart;
	    int pEnd = pStart + parent->qSize;
	    int intersection = rangeIntersection(fStart, fEnd, pStart, pEnd);
	    if (intersection > 0)
	        {
		fill->qOver = intersection;
		fill->qFar = 0;
		}
	    else
	        {
		fill->qOver = 0;
		fill->qFar = -intersection;
		}
	    if (parent->qStrand == fill->qStrand)
	        type = "syn";
	    else
	        type = "inv";
	    }
	fill->type = type;
	}
    if (fill->children)
        rNetSyn(net, qChromHash, fill->children, fill);
    }
}

void netSyntenic(char *inFile, char *outFile)
/* netSyntenic - Add synteny info to net. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct chainNet *netList = NULL, *net;
struct hash *qChromHash = newHash(21);

lineFileSetMetaDataOutput(lf, f);

lm = lmInit(0);
lmAllocArray(lm, rbStack, 256);

/* Read in net. */
while ((net = chainNetRead(lf)) != NULL)
    {
    slAddHead(&netList, net);
    }
slReverse(&netList);

/* Build up duplication structure. */
for (net = netList; net != NULL; net = net->next)
    {
    rCalcDupes(net, qChromHash, net->fillList);
    }

for (net = netList; net != NULL; net = net->next)
    {
    rNetSyn(net, qChromHash, net->fillList, NULL);
    chainNetWrite(net, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
netSyntenic(argv[1], argv[2]);
printMem();
return 0;
}
