/* chainNet - Make alignment nets out of chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "rbTree.h"
#include "chainBlock.h"

int minSpace = 25;	/* Minimum gap size to fill. */
int minScore = 2000;	/* Minimum chain score to look at. */

boolean uglier = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainNet - Make alignment nets out of chains\n"
  "usage:\n"
  "   chainNet in.chain target.sizes query.sizes target.net query.net\n"
  "options:\n"
  "   -minSpace=N - minimum gap size to fill, default %d\n"
  "   -minScore=N - minimum chain score to consider, default %d\n",
  minSpace, minScore);
}

struct gap
/* A gap in sequence alignments. */
    {
    struct gap *next;	    /* Next gap in list. */
    int start,end;	    /* Range covered in chromosome. */
    struct fill *fillList;  /* List of gap fillers. */
    };

struct fill
/* Some alignments that hopefully fill some gaps. */
    {
    struct fill *next;	   /* Next fill in list. */
    int start,end;	   /* Range covered, always plus strand. */
    struct gap *gapList;   /* List of internal gaps. */
    struct chain *chain;   /* Alignment chain (not all is necessarily used) */
    };

struct space
/* A part of a gap. */
    {
    struct gap *gap;    /* The actual gap. */
    int start, end;	/* Portion of gap covered, always plus strand. */
    };

struct chrom
/* A chromosome. */
    {
    struct chrom *next;	    /* Next in list. */
    char *name;		    /* Name - allocated in hash */
    int size;		    /* Size of chromosome. */
    struct gap *root;	    /* Root of the gap/chain tree */
    struct rbTree *spaces;  /* Store gaps here for fast lookup. */
    };


int gapCmpStart(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct gap *a = *((struct gap **)va);
const struct gap *b = *((struct gap **)vb);
return a->start - b->start;
}

int fillCmpStart(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct fill *a = *((struct fill **)va);
const struct fill *b = *((struct fill **)vb);
return a->start - b->start;
}

int cmpSpaceRef(const void *va, const void *vb)
/* Compare to sort based on start. */
{
const struct slRef *ra = *((struct slRef **)va);
const struct slRef *rb = *((struct slRef **)vb);
struct space *a = ra->val;
struct space *b = rb->val;
return a->start - b->start;
}


int spaceCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct space *a = va;
struct space *b = vb;
if (a->end <= b->start)
    return -1;
else if (b->end <= a->start)
    return 1;
else
    return 0;
}

void dumpSpace(void *item, FILE *f)
/* Print out range info. */
{
struct space *space = item;
fprintf(f, "%d,%d", space->start, space->end);
}

void doSpace(void *item)
/* Do something to range. */
{
struct space *space = item;
printf("%d,%d\n", space->start, space->end);
}

void testRanges()
{
static int testStarts[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
static int testEnds[]   = {5, 15, 25, 35, 45, 60, 70, 80, 90, 99};
int i;
struct space *r, s;
struct rbTree *tree = rbTreeNew(spaceCmp);

for (i=0; i<ArraySize(testStarts); ++i)
    {
    AllocVar(r);
    r->start = testStarts[i];
    r->end = testEnds[i];
    rbTreeAdd(tree, r);
    }
rbTreeDump(tree, uglyOut, dumpSpace);

s.start = 20;
s.end = 81;
uglyf("rbTreeTraverse(%d %d)\n", s.start, s.end);
rbTreeTraverseRange(tree, &s, &s, doSpace);
}

struct gap *addGap(struct chrom *chrom, char strand,
	int minStart, int maxEnd, int start, int end)
/* Add gap to chromosome. */
{
struct gap *gap;
struct space *space;
if (strand == '-')
    reverseIntRange(&start, &end, chrom->size);
if (start <= minStart) return NULL;
if (end >= maxEnd) return NULL;
if (end - start < minSpace) return NULL;
AllocVar(gap);
AllocVar(space);
space->gap = gap;
space->start = gap->start = start;
space->end = gap->end = end;
rbTreeAdd(chrom->spaces, space);
return gap;
}

void makeChroms(char *sizeFile, struct hash **retHash, struct chrom **retList)
/* Read size file and make chromosome structure for each
 * element. */
{
struct lineFile *lf = lineFileOpen(sizeFile, TRUE);
char *row[2];
struct hash *hash = newHash(0);
struct chrom *chrom, *chromList = NULL;
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    if (hashLookup(hash, name) != NULL)
        errAbort("Duplicate %s line %d of %s", name, lf->lineIx, lf->fileName);
    AllocVar(chrom);
    slAddHead(&chromList, chrom);
    hashAddSaveName(hash, name, chrom, &chrom->name);
    chrom->size = lineFileNeedNum(lf, row, 1);
    chrom->spaces = rbTreeNew(spaceCmp);
    chrom->root = addGap(chrom, '+', -1, chrom->size+1, 0, chrom->size);
    }
lineFileClose(&lf);
slReverse(&chromList);
*retHash = hash;
*retList = chromList;
}

boolean innerBounds(struct chain *chain, boolean isQ, 
	int inStart, int inEnd, int *retStart, int *retEnd)
/* Return the portion of chain which is between inStart and
 * inEnd.  Only considers blocks inbetween inStart and inEnd.  
 * Return NULL if chain has no blocks between these. */
{
struct boxIn *b;
int start = BIGNUM, end = -BIGNUM;
char strand = (isQ ? chain->qStrand : '+');
boolean isRev = (strand == '-');
if (isRev)
    reverseIntRange(&inStart, &inEnd, chain->qSize);
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int s, e;
    if (isQ)
       {
       s = b->qStart;
       e = b->qEnd;
       }
    else
       {
       s = b->tStart;
       e = b->tEnd;
       }
    if (e <= inStart)
        continue;
    if (s >= inEnd)
        break;
    if (s < inStart) s = inStart;
    if (e > inEnd) s = inEnd;
    if (start > s) start = s;
    if (end < e) end = e;
    }
if (start > end)
    return FALSE;
if (isRev)
    reverseIntRange(&start, &end, chain->qSize);
*retStart = start;
*retEnd = end;
return TRUE;
}

struct fill *fillSpace(struct chrom *chrom, struct space *space, 
	struct chain *chain, boolean isQ)
/* Fill in space with chain, remove existing space from chrom,
 * and add smaller spaces on either side if big enough. */
{
struct fill *fill;
int s, e;
struct space *lSpace, *rSpace;

if (!innerBounds(chain, isQ, space->start, space->end, &s, &e))
    return NULL;
assert(s < e);
AllocVar(fill);
fill->start = s;
fill->end = e;
fill->chain = chain;
rbTreeRemove(chrom->spaces, space);
if (s - space->start >= minSpace)
    {
    AllocVar(lSpace);
    lSpace->gap = space->gap;
    lSpace->start = space->start;
    lSpace->end = s;
    rbTreeAdd(chrom->spaces, lSpace);
    }
if (space->end - e >= minSpace)
    {
    AllocVar(rSpace);
    rSpace->gap = space->gap;
    rSpace->start = e;
    rSpace->end = space->end;
    rbTreeAdd(chrom->spaces, rSpace);
    }
slAddHead(&space->gap->fillList, fill);
return fill;
}

static struct slRef *fsList;

void fsAdd(void *item)
/* Add item to fsList. */
{
refAdd(&fsList, item);
}

struct slRef *findSpaces(struct rbTree *tree, int start, int end)
/* Return a list of subGaps that intersect interval between start
 * and end. */
{
static struct space space;
space.start = start;
space.end = end;
fsList = NULL;
rbTreeTraverseRange(tree, &space, &space, fsAdd);
return fsList;
}

void addChain(struct chrom *qChrom, struct chrom *tChrom, struct chain *chain)
/* Add as much of chain as possible to chromosomes. */
{
struct slRef *spaceList;
struct slRef *ref;
struct boxIn *block, *nextBlock;
struct gap *gap;
boolean isRev = (chain->qStrand == '-'); 
int qStart = chain->qStart, qEnd = chain->qEnd;

if (isRev)
    reverseIntRange(&qStart, &qEnd, chain->qSize);
spaceList = findSpaces(qChrom->spaces,qStart,qEnd);
#ifdef DEBUG
if (uglier) uglyf("add chain on q\n");
if (uglier)
    {
    struct slRef *fullSpaceList = findSpaces(qChrom->spaces, 0, qChrom->size);
    struct slRef *ref;
    slSort(&fullSpaceList, cmpSpaceRef);
    for (ref = fullSpaceList; ref != NULL; ref = ref->next)
        {
	struct space *space = ref->val;
	uglyf("  %d-%d\n", space->start, space->end);
	}
    }
#endif /* DEBUG */
uglyf("Got %d qSpaces\n", slCount(spaceList));
for (ref = spaceList; ref != NULL; ref = ref->next)
    {
    struct space *space = ref->val;
    struct fill *fill;
    if ((fill = fillSpace(qChrom, space, chain, TRUE)) != NULL)
	{
	for (block = chain->blockList; ; block = nextBlock)
	    {
	    nextBlock = block->next;
	    if (nextBlock == NULL)
		break;
	    if ((gap = addGap(qChrom, chain->qStrand, space->start, space->end, block->qEnd, nextBlock->qStart)) != NULL)
		{
		slAddHead(&fill->gapList, gap);
		}
	    }
	freez(&ref->val);
	}
    }
slFreeList(&spaceList);

spaceList = findSpaces(tChrom->spaces,chain->tStart,chain->tEnd);
uglyf("Got %d tSpaces\n", slCount(spaceList));
for (ref = spaceList; ref != NULL; ref = ref->next)
    {
    struct space *space = ref->val;
    struct fill *fill;
    if ((fill = fillSpace(tChrom, space, chain, FALSE)) != NULL)
	{
	for (block = chain->blockList; ; block = nextBlock)
	    {
	    nextBlock = block->next;
	    if (nextBlock == NULL)
		break;
	    if ((gap = addGap(tChrom, '+', space->start, space->end, block->tEnd, nextBlock->tStart)) != NULL)
		{
		slAddHead(&fill->gapList, gap);
		}
	    }
	freez(&ref->val);
	}
    }
slFreeList(&spaceList);
}

int rOutDepth = 0;
boolean rOutQ = FALSE;

static void rOutputGap(struct gap *gap, FILE *f);
static void rOutputFill(struct fill *fill, FILE *f);

static void rOutputGap(struct gap *gap, FILE *f)
/* Recursively output gap and it's fillers. */
{
struct fill *fill;
++rOutDepth;
spaceOut(f, rOutDepth);
fprintf(f, "gap %d %d %d\n", gap->start, gap->end, gap->end - gap->start);
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    rOutputFill(fill, f);
--rOutDepth;
}

static void qClipChainOut(FILE *f, struct chain *chain, 
	int clipStart, int clipEnd)
/* Output subsection of chain bounded by clipStart/clipEnd on q coordinates. 
 * clipStart/clipEnd are always in plus strand coordinates. */
{
boolean isRev = (chain->qStrand == '-');
int tMin = BIGNUM, tMax = -BIGNUM;
int qMin = BIGNUM, qMax = -BIGNUM;
struct boxIn *b;
if (isRev)
    reverseIntRange(&clipStart, &clipEnd, chain->qSize);
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int qs, qe, ts, te;	/* Clipped bounds of block */
    if ((qe = b->qEnd) <= clipStart)
        continue;
    if ((qs = b->qStart) >= clipEnd)
        break;
    ts = b->tStart;
    te = b->tEnd;
    if (qs < clipStart)
        {
	ts += (clipStart - qs);
	qs = clipStart;
	}
    if (qe > clipEnd)
        {
	te  -= (clipEnd - qe);
	qe = clipEnd;
	}
    if (qMin > qs) qMin = qs;
    if (qMax < qe) qMax = qe;
    if (tMin > ts) tMin = ts;
    if (tMax < te) tMax = te;
    }
if (tMin < tMax)
    {
    if (isRev)
        reverseIntRange(&qMin, &qMax, chain->qSize);
    spaceOut(f, rOutDepth);
    fprintf(f, "fill %d %d %d %s %c %d %d %d\n", qMin, qMax, qMax-qMin, 
    	chain->tName, chain->qStrand, tMin, tMax, tMax-tMin);
    }
}

static void tClipChainOut(FILE *f, struct chain *chain, 
	int clipStart, int clipEnd)
/* Output subsection of chain bounded by clipStart/clipEnd on t coordinates. */
{
boolean isRev = (chain->qStrand == '-');
int tMin = BIGNUM, tMax = -BIGNUM;
int qMin = BIGNUM, qMax = -BIGNUM;
struct boxIn *b;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int qs, qe, ts, te;	/* Clipped bounds of block */
    if ((te = b->tEnd) <= clipStart)
        continue;
    if ((ts = b->tStart) >= clipEnd)
        break;
    qs = b->qStart;
    qe = b->qEnd;
    if (ts < clipStart)
        {
	qs += (clipStart - ts);
	ts = clipStart;
	}
    if (te > clipEnd)
        {
	qe  -= (clipEnd - te);
	te = clipEnd;
	}
    if (qMin > qs) qMin = qs;
    if (qMax < qe) qMax = qe;
    if (tMin > ts) tMin = ts;
    if (tMax < te) tMax = te;
    }
if (qMin < qMax)
    {
    if (isRev)
        reverseIntRange(&qMin, &qMax, chain->qSize);
    spaceOut(f, rOutDepth);
    fprintf(f, "fill %d %d %d %s %c %d %d %d\n", tMin, tMax, tMax-tMin, 
    	chain->qName, chain->qStrand, qMin, qMax, qMax-qMin);
    }
}


static void rOutputFill(struct fill *fill, FILE *f)
/* Recursively output fill and it's gaps. */
{
struct gap *gap;
struct chain *chain = fill->chain;
++rOutDepth;
if (rOutQ)
    {
    qClipChainOut(f, chain, fill->start, fill->end);
    }
else
    {
    tClipChainOut(f, chain, fill->start, fill->end);
    }
for (gap = fill->gapList; gap != NULL; gap = gap->next)
    rOutputGap(gap, f);
--rOutDepth;
}

void sortNet(struct gap *gap)
/* Recursively sort lists. */
{
struct fill *fill;
struct gap *g;
slSort(&gap->fillList, fillCmpStart);
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    {
    slSort(&fill->gapList, gapCmpStart);
    for (gap = fill->gapList; gap != NULL; gap = gap->next)
        sortNet(gap);
    }
}

void outputNetSide(struct chrom *chromList, char *fileName, boolean isQ)
/* Output one side of net */
{
FILE *f = mustOpen(fileName, "w");
struct chrom *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    rOutDepth = 0;
    rOutQ = isQ;
    fprintf(f, "net %s %d\n", chrom->name, chrom->size);
    if (chrom->root != NULL)
	{
	sortNet(chrom->root);
	rOutputGap(chrom->root, f);
	}
    }
}

void chainNet(char *chainFile, char *tSizes, char *qSizes, char *tNet, char *qNet)
/* chainNet - Make alignment nets out of chains. */
{
struct lineFile *lf = lineFileOpen(chainFile, TRUE);
struct hash *qHash, *tHash;
struct chrom *qChromList, *tChromList, *tChrom, *qChrom;
struct chain *chain;

makeChroms(qSizes, &qHash, &qChromList);
makeChroms(tSizes, &tHash, &tChromList);
printf("Got %d chroms in %s, %d in %s\n", slCount(tChromList), tSizes,
	slCount(qChromList), qSizes);
while ((chain = chainRead(lf)) != NULL)
    {
    if (chain->score < minScore) 
    	break;
    uglyf("chain %f (%d els) %s %d-%d %c %s %d-%d\n", chain->score, slCount(chain->blockList), chain->tName, chain->tStart, chain->tEnd, chain->qStrand, chain->qName, chain->qStart, chain->qEnd);
    qChrom = hashMustFindVal(qHash, chain->qName);
    if (qChrom->size != chain->qSize)
        errAbort("%s is %d in %s but %d in %s", chain->qName, 
		chain->qSize, chainFile,
		qChrom->size, qSizes);
    tChrom = hashMustFindVal(tHash, chain->tName);
    if (tChrom->size != chain->tSize)
        errAbort("%s is %d in %s but %d in %s", chain->tName, 
		chain->tSize, chainFile,
		tChrom->size, tSizes);
    addChain(qChrom, tChrom, chain);
    uglyf("%s has %d inserts, %s has %d\n", tChrom->name, tChrom->spaces->n, qChrom->name, qChrom->spaces->n);
    }
uglyf("About to output\n");
outputNetSide(tChromList, tNet, FALSE);
outputNetSide(qChromList, qNet, TRUE);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 6)
    usage();
minSpace = optionInt("minSpace", minSpace);
minScore = optionInt("minScore", minScore);
chainNet(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
