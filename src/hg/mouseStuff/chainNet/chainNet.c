/* chainNet - Make alignment nets out of chains. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "rbTree.h"
#include "chainBlock.h"
#include "jksql.h"
#include "hdb.h"
#include "localmem.h"
#include "agpGap.h"

int minSpace = 25;	/* Minimum gap size to fill. */
int minFill;		/* Minimum fill to record. */
int minScore = 2000;	/* Minimum chain score to look at. */
double oGapRatio = 0.5; /* Minimum ratio of coverage by N's in other
                         * sequence for a gap to be considered an oGap. */
boolean verbose = FALSE;/* Verbose output. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainNet - Make alignment nets out of chains\n"
  "usage:\n"
  "   chainNet targetDb queryDb in.chain target.net query.net target.flat query.flat\n"
  "where:\n"
  "   targetDb is the target genome database (example hg12)\n"
  "   queryDb is the query genome database (example mm2)\n"
  "   in.chain is the chain file sorted by score\n"
  "   target.net is the output over the target genome\n"
  "   query.net is the output over the query genome\n"
  "options:\n"
  "   -minSpace=N - minimum gap size to fill, default %d\n"
  "   -minFill=N  - default half of minSpace\n"
  "   -minScore=N - minimum chain score to consider, default %d\n"
  "   -oGapRatio=0.N - minimum coverage by N's in other sequence for\n"
  "                    a gap to be classified an 'oGap'. Default %3.1f\n"
  "   -verbose - make copious output\n"
  "   -tFlat=target.flat - Create a flattened, classified file of regions in target\n"
  "   -qFlat=query.flat  - Create flattened, classified file of regions in query\n"
  , minSpace, minScore, oGapRatio);
}

struct range
/* A part of a chromosome. */
    {
    int start, end;	/* Half open zero based coordinates. */
    };

struct gap
/* A gap in sequence alignments. */
    {
    struct gap *next;	    /* Next gap in list. */
    int start,end;	    /* Range covered in chromosome. */
    struct fill *fillList;  /* List of gap fillers. */
    bool oGap;	    	    /* True if gap corresponds to sequence gap in other
                             * sequence. */
    };

struct fill
/* Some alignments that hopefully fill some gaps. */
    {
    struct fill *next;	   /* Next fill in list. */
    int start,end;	   /* Range covered, always plus strand. */
    int oStart,oEnd;	   /* Range covered in other coordinate, always plus. */
    struct gap *gapList;   /* List of internal gaps. */
    struct chain *chain;   /* Alignment chain (not all is necessarily used) */
    };

struct space
/* A part of a gap. */
    {
    int start, end;	/* Portion of gap covered, always plus strand. */
                        /* The start of this structure has to be identical
			 * with struct range above. */
    struct gap *gap;    /* The actual gap. */
    };

struct chrom
/* A chromosome. */
    {
    struct chrom *next;	    /* Next in list. */
    char *name;		    /* Name - allocated in hash */
    int size;		    /* Size of chromosome. */
    struct gap *root;	    /* Root of the gap/chain tree */
    struct rbTree *spaces;  /* Store gaps here for fast lookup. Items are spaces. */
    struct rbTree *seqGaps; /* Gaps (stretches of Ns) in sequence.  Items are ranges.*/
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

int rangeCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct range *a = va;
struct range *b = vb;
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

void addSpaceForGap(struct chrom *chrom, struct gap *gap)
/* Given a gap, create corresponding space in chromosome's
 * space rbTree. */
{
struct space *space;
AllocVar(space);
space->gap = gap;
space->start = gap->start;
space->end = gap->end;
rbTreeAdd(chrom->spaces, space);
}

struct gap *gapNew(int start, int end)
/* Create a new gap. */
{
struct gap *gap;
AllocVar(gap);
gap->start = start;
gap->end = end;
return gap;
}

struct gap *addGap(struct chrom *chrom, int start, int end)
/* Create a new gap and add it to chromosome space tree. */
{
struct gap *gap = gapNew(start, end);
addSpaceForGap(chrom, gap);
return gap;
}

boolean strictlyInside(int minStart, int maxEnd, int start, int end)
/* Return TRUE if minStart < start <= end < maxEnd 
 * and end - start >= minSpace */
{
return (minStart < start && start + minSpace <= end && end < maxEnd);
}

struct rbTree *getSeqGaps(char *db, char *chrom, struct lm *lm)
/* Return a tree of ranges for sequence gaps in chromosome */
{
struct sqlConnection *conn = sqlConnect(db);
struct rbTree *tree = rbTreeNew(rangeCmp);
int rowOffset;
struct sqlResult *sr = hChromQuery(conn, "gap", chrom, NULL, &rowOffset);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct agpGap gap;
    struct range *range;
    agpGapStaticLoad(row+rowOffset, &gap);
    lmAllocVar(lm, range);
    range->start = gap.chromStart;
    range->end = gap.chromEnd;
    rbTreeAdd(tree, range);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tree;
}


void makeChroms(char *db, struct lm *lm,
	struct hash **retHash, struct chrom **retList)
/* Read size file and make chromosome structure for each
 * element.  Read in gaps and repeats. */
{
char **row;
struct hash *hash = newHash(0);
struct chrom *chrom, *chromList = NULL;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = sqlGetResult(conn, "select chrom,size from chromInfo");

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (hashLookup(hash, name) != NULL)
        errAbort("Duplicate %s in %s.chromInfo", name, db);
    AllocVar(chrom);
    slAddHead(&chromList, chrom);
    hashAddSaveName(hash, name, chrom, &chrom->name);
    chrom->size = sqlUnsigned(row[1]);
    chrom->spaces = rbTreeNew(spaceCmp);
    chrom->root = addGap(chrom, 0, chrom->size);
    chrom->seqGaps = getSeqGaps(db, chrom->name, lm);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&chromList);
*retHash = hash;
*retList = chromList;
}

boolean innerBounds(struct boxIn *startBlock,
	boolean isQ, int inStart, int inEnd, int *retStart, int *retEnd)
/* Return the portion of chain which is between inStart and
 * inEnd.  Only considers blocks inbetween inStart and inEnd.  
 * Return NULL if chain has no blocks between these. */
{
struct boxIn *b;
int start = BIGNUM, end = -BIGNUM;
for (b = startBlock; b != NULL; b = b->next)
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
    if (e > inEnd) e = inEnd;
    if (start > s) start = s;
    if (end < e) end = e;
    }
if (end - start < minFill)
    return FALSE;
*retStart = start;
*retEnd = end;
return TRUE;
}

static void qFillOtherRange(struct fill *fill)
/* Given bounds of fill in q coordinates, calculate
 * oStart/oEnd in t coordinates, and refine 
 * start/end to reflect parts of chain actually used. */
{
struct chain *chain = fill->chain;
int clipStart = fill->start;
int clipEnd = fill->end;
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
if (isRev)
    reverseIntRange(&qMin, &qMax, chain->qSize);
fill->start = qMin;
fill->end = qMax;
fill->oStart = tMin;
fill->oEnd = tMax;
assert(tMin < tMax);
}

static void tFillOtherRange(struct fill *fill)
/* Given bounds of fill in t coordinates, calculate
 * oStart/oEnd in q coordinates, and refine 
 * start/end to reflect parts of chain actually used. */
{
struct chain *chain = fill->chain;
int clipStart = fill->start;
int clipEnd = fill->end;
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
if (isRev)
    reverseIntRange(&qMin, &qMax, chain->qSize);
fill->start = tMin;
fill->end = tMax;
fill->oStart = qMin;
fill->oEnd = qMax;
assert(qMin < qMax);
}


struct fill *fillSpace(struct chrom *chrom, struct space *space, 
	struct chain *chain, struct boxIn *startBlock, 
	boolean isQ)
/* Fill in space with chain, remove existing space from chrom,
 * and add smaller spaces on either side if big enough. */
{
struct fill *fill;
int s, e;
struct space *lSpace, *rSpace;

if (!innerBounds(startBlock, isQ, space->start, space->end, &s, &e))
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
/* Return a list of spaces that intersect interval between start
 * and end. */
{
static struct space space;
space.start = start;
space.end = end;
fsList = NULL;
rbTreeTraverseRange(tree, &space, &space, fsAdd);
slReverse(&fsList);
return fsList;
}

struct slRef *findRanges(struct rbTree *tree, int start, int end)
/* Return list of ranges that intersect interval. */
{
return findSpaces(tree, start, end);
}


void reverseBlocksQ(struct boxIn **pList, int qSize)
/* Reverse qside of blocks. */
{
struct boxIn *b;
slReverse(pList);
for (b = *pList; b != NULL; b = b->next)
    reverseIntRange(&b->qStart, &b->qEnd, qSize);
}

int intersectingGapSize(struct chrom *chrom, int start, int end)
/* Return total size of gaps intersecting ranges start-end. */
{
struct slRef *refList = findRanges(chrom->seqGaps, start, end);
struct slRef *ref;
int size, totalSize = 0;

for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct range *range = ref->val;
    size = rangeIntersection(start, end, range->start, range->end);
    assert(size > 0);
    totalSize += size;
    }
slFreeList(&refList);
return totalSize;
}


boolean oSeqPlugged(struct chrom *otherChrom, int otherStart, int otherEnd,
	int thisStart, int thisEnd)
/* Return TRUE if gap could plausibly be explained by missing
 * sequence on the other chromosome. */
{
int otherSize = otherEnd - otherStart;
int thisSize = thisEnd - thisStart;
int minSize = min(otherSize, thisSize);
int maxSize = max(otherSize, thisSize);
int oGapSize;
boolean isPlugged;
if (otherSize < 100)  /* Smallest gap size */
    return FALSE;
if (minSize * 2 + 2000 < maxSize) /* 2x expansion plus one LINE */
    return FALSE;
oGapSize = intersectingGapSize(otherChrom, otherStart, otherEnd);
isPlugged = (oGapSize >= oGapRatio * otherSize);
return isPlugged;
}


void addChainT(struct chrom *chrom, struct chrom *otherChrom, struct chain *chain)
/* Add T side of chain to fill/gap tree of chromosome. 
 * This is the easier case since there are no strand
 * issues to worry about. */
{
struct slRef *spaceList;
struct slRef *ref;
struct boxIn *startBlock, *block, *nextBlock;
struct gap *gap;
struct fill *fill = NULL;

spaceList = findSpaces(chrom->spaces,chain->tStart,chain->tEnd);
startBlock = chain->blockList;
for (ref = spaceList; ref != NULL; ref = ref->next)
    {
    struct space *space = ref->val;
    struct fill *fill;
    int gapStart, gapEnd;
    for (;;)
        {
	nextBlock = startBlock->next;
	if (nextBlock == NULL)
	    break;
	gapEnd = nextBlock->tStart;
	if (gapEnd > space->start)
	    break;
	startBlock = nextBlock;
	}
    if ((fill = fillSpace(chrom, space, chain, startBlock, FALSE)) != NULL)
	{
	for (block = startBlock; ; block = nextBlock)
	    {
	    nextBlock = block->next;
	    if (nextBlock == NULL)
		break;
	    gapStart = block->tEnd;
	    gapEnd = nextBlock->tStart;
	    if (strictlyInside(space->start, space->end, gapStart, gapEnd))
		{
		gap = gapNew(gapStart, gapEnd);
		if (oSeqPlugged(otherChrom, block->qEnd, 
			nextBlock->qStart, gapStart, gapEnd))
		    gap->oGap = TRUE;
		else
		    addSpaceForGap(chrom, gap);
		slAddHead(&fill->gapList, gap);
		}
	    }
	freez(&ref->val);	/* aka space */
	}
    }
slFreeList(&spaceList);
}

void addChainQ(struct chrom *chrom, struct chrom *otherChrom, struct chain *chain)
/* Add Q side of chain to fill/gap tree of chromosome. 
 * For this side we have to cope with reverse strand
 * issues. */
{
struct slRef *spaceList;
struct slRef *ref;
struct boxIn *startBlock, *block, *nextBlock;
int gapStart, gapEnd;
struct gap *gap;
struct fill *fill = NULL;
boolean isRev = (chain->qStrand == '-'); 
int qStart = chain->qStart, qEnd = chain->qEnd;

if (isRev)
    {
    reverseIntRange(&qStart, &qEnd, chain->qSize);
    reverseBlocksQ(&chain->blockList, chain->qSize);
    }
spaceList = findSpaces(chrom->spaces,qStart,qEnd);
startBlock = chain->blockList;

for (ref = spaceList; ref != NULL; ref = ref->next)
    {
    struct space *space = ref->val;
    struct fill *fill;
    for (;;)
        {
	nextBlock = startBlock->next;
	if (nextBlock == NULL)
	    break;
	gapEnd = nextBlock->qStart;
	if (gapEnd > space->start)
	    break;
	startBlock = nextBlock;
	}
    if ((fill = fillSpace(chrom, space, chain, startBlock, TRUE)) 
    	!= NULL)
	{
	for (block = startBlock; ; block = nextBlock)
	    {
	    nextBlock = block->next;
	    if (nextBlock == NULL)
		break;
	    gapStart = block->qEnd;
	    gapEnd = nextBlock->qStart;
	    if (strictlyInside(space->start, space->end, gapStart, gapEnd))
		{
		gap = gapNew(gapStart, gapEnd);
		if (oSeqPlugged(otherChrom, block->tEnd, nextBlock->tStart, 
			gapStart, gapEnd))
		    gap->oGap = TRUE;
		else
		    addSpaceForGap(chrom, gap);
		slAddHead(&fill->gapList, gap);
		}
	    }
	freez(&ref->val);	/* aka space */
	}
    }
slFreeList(&spaceList);
if (isRev)
    reverseBlocksQ(&chain->blockList, chain->qSize);
}

void addChain(struct chrom *qChrom, struct chrom *tChrom, struct chain *chain)
/* Add as much of chain as possible to chromosomes. */
{
addChainQ(qChrom, tChrom, chain);
addChainT(tChrom, qChrom, chain);
}

char *gapType(struct gap *gap)
/* Return name of gap type */
{
if (gap->oGap)
    return "oGap";
else
    return "gap";
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
fprintf(f, "%s", gapType(gap));
fprintf(f, " %d %d %d\n", gap->start, gap->end, gap->end - gap->start);
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    rOutputFill(fill, f);
--rOutDepth;
}

void fillOut(FILE *f, struct fill *fill, char *oChrom, int depth)
/* Output fill. */
{
struct chain *chain = fill->chain;
spaceOut(f, depth);
fprintf(f, "fill %d %d %d %s %c %d %d %d id %d\n", 
	fill->start, fill->end, fill->end - fill->start,
	oChrom, chain->qStrand, 
	fill->oStart, fill->oEnd, fill->oEnd - fill->oStart,
	chain->id);
}

static void rOutputFill(struct fill *fill, FILE *f)
/* Recursively output fill and it's gaps. */
{
struct gap *gap;
struct chain *chain = fill->chain;
++rOutDepth;
if (rOutQ)
    {
    qFillOtherRange(fill);
    fillOut(f, fill, fill->chain->tName, rOutDepth);
    }
else
    {
    tFillOtherRange(fill);
    fillOut(f, fill, fill->chain->qName, rOutDepth);
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

boolean chromHasData(struct chrom *chrom)
/* Return TRUE if chrom is non-empty */
{
return chrom != NULL && chrom->root != NULL && chrom->root->fillList != NULL;
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
    if (chromHasData(chrom))
	{
	fprintf(f, "net %s %d\n", chrom->name, chrom->size);
	sortNet(chrom->root);
	rOutputGap(chrom->root, f);
	}
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
        printf("memory usage %s, utime %s s/100, stime %s\n", 
		words[22], words[13], words[14]);
    }
lineFileClose(&lf);
}

void compactChromList(struct chrom *chromList)
/* Free up bits of chromosome structure not needed for 
 * classification and output. */
{
struct chrom *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    rbTreeFree(&chrom->spaces);
}

void rFlatGapOut(struct gap *gap, char *chromName, FILE *f, 
	int depth, int parentId, boolean isQuery);

void writeFlatFill(FILE *f, char *chrom, int start, int end, int depth, int parentId, int id,
	struct chain *chain, boolean isQuery)
/* Write out flattened fill record. */
{
fprintf(f, "%s\t%d\t%d\tfill\t%d\t%d\t%d\t%s\t%c\n", 
	chrom, start, end, depth, parentId, id,
	(isQuery ? chain->tName : chain->qName),
	chain->qStrand);
}

void rFlatFillOut(struct fill *fill, char *chromName, FILE *f,
        int depth, int parentId, boolean isQuery)
/* Write out flattened fill recursively. */
{
int lastEnd = fill->start;
struct gap *gap;
struct chain *chain = fill->chain;
int id = chain->id;
for (gap = fill->gapList; gap != NULL; gap = gap->next)
    {
    if (gap->start != lastEnd)
        {
	writeFlatFill(f, chromName, lastEnd, gap->start, depth, parentId, id, chain, isQuery);
	}
    lastEnd = gap->end;
    rFlatGapOut(gap, chromName, f, depth+1, id, isQuery);
    }
if (lastEnd != fill->end)
    {
    writeFlatFill(f, chromName, lastEnd, fill->end, depth, parentId, id, chain, isQuery);
    }
}

void rFlatGapOut(struct gap *gap, char *chromName, FILE *f, 
	int depth, int parentId, boolean isQuery)
/* Write out flattened gap recursively. */
{
int lastEnd = gap->start;
struct fill *fill;
for (fill = gap->fillList; fill != NULL; fill = fill->next)
    {
    if (fill->start != lastEnd)
        {
	fprintf(f, "%s\t%d\t%d\t%s\t%d\t%d\n", chromName, lastEnd, fill->start, 
		gapType(gap), depth, parentId);
	}
    rFlatFillOut(fill, chromName, f, depth, parentId, isQuery);
    lastEnd = fill->end;
    }
if (lastEnd != gap->end)
    {
    fprintf(f, "%s\t%d\t%d\t%s\t%d\t%d\n", chromName, lastEnd, gap->end, 
	    gapType(gap), depth, parentId);
    }
}

void flatOut(struct chrom *chromList, char *fileName, boolean isQuery)
/* Output flattened, classified data. */
{
FILE *f = mustOpen(fileName, "w");
struct chrom *chrom;

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (chromHasData(chrom))
	rFlatGapOut(chrom->root, chrom->name, f, 0, 0, isQuery);
    }
carefulClose(&f);
}


void chainNet(char *tDb, char *qDb, char *chainFile, char *tNet, char *qNet)
/* chainNet - Make alignment nets out of chains. */
{
struct lineFile *lf = lineFileOpen(chainFile, TRUE);
struct hash *qHash, *tHash;
struct chrom *qChromList, *tChromList, *tChrom, *qChrom;
struct chain *chain;
struct lm *lm = lmInit(0);	/* Local memory for chromosome data. */

makeChroms(qDb, lm, &qHash, &qChromList);
makeChroms(tDb, lm, &tHash, &tChromList);
printf("Got %d chroms in %s, %d in %s\n", slCount(tChromList), tDb,
	slCount(qChromList), qDb);

/* Loop through chain file building up net. */
while ((chain = chainRead(lf)) != NULL)
    {
    if (chain->score < minScore) 
    	break;
    if (verbose)
	{
	printf("chain %f (%d els) %s %d-%d %c %s %d-%d\n", 
		chain->score, slCount(chain->blockList), 
		chain->tName, chain->tStart, chain->tEnd, 
		chain->qStrand, chain->qName, chain->qStart, chain->qEnd);
	}
    qChrom = hashMustFindVal(qHash, chain->qName);
    if (qChrom->size != chain->qSize)
        errAbort("%s is %d in %s but %d in %s", chain->qName, 
		chain->qSize, chainFile,
		qChrom->size, qDb);
    tChrom = hashMustFindVal(tHash, chain->tName);
    if (tChrom->size != chain->tSize)
        errAbort("%s is %d in %s but %d in %s", chain->tName, 
		chain->tSize, chainFile,
		tChrom->size, tDb);
    addChain(qChrom, tChrom, chain);
    if (verbose)
	printf("%s has %d inserts, %s has %d\n", tChrom->name, 
		tChrom->spaces->n, qChrom->name, qChrom->spaces->n);
    }

/* Free up some stuff no longer needed. */
printf("Compacting memory\n");
compactChromList(tChromList);
compactChromList(qChromList);

/* Write out basic net files. */
printf("writing %s\n", tNet);
outputNetSide(tChromList, tNet, FALSE);
printf("writing %s\n", qNet);
outputNetSide(qChromList, qNet, TRUE);

/* Optionally write out flattened classification files. */
if (optionExists("tFlat"))
   flatOut(tChromList, optionVal("tFlat", NULL), FALSE);
if (optionExists("qFlat"))
   flatOut(qChromList, optionVal("qFlat", NULL), TRUE);
printMem();
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 6)
    usage();
minSpace = optionInt("minSpace", minSpace);
minFill = optionInt("minFill", minSpace/2);
minScore = optionInt("minScore", minScore);
oGapRatio = optionFloat("oGapRatio", oGapRatio);
verbose = optionExists("verbose");
chainNet(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
