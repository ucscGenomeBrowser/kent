/* axtChain - Chain together axt alignments.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "axt.h"
#include "boxClump.h"
#include "chainBlock.h"
#include "portable.h"

int minScore = 400;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtChain - Chain together axt alignments.\n"
  "usage:\n"
  "   axtChain in.axt tNibDir qNibDir out.chain\n"
  "options:\n"
  "   -minScore=N  Minimum score for chain, default 400\n"
  );
}

struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct boxIn *blockList; /* List of alignments. */
    int axtCount;		/* Count of axt's that make this up (just for stats) */
    };

void addAxtBlocks(struct boxIn **pList, struct axt *axt)
/* Add blocks (gapless subalignments) from axt to block list. */
{
boolean thisIn, lastIn = FALSE;
int qPos = axt->qStart, tPos = axt->tStart;
int qStart = 0, tStart = 0;
int i;

for (i=0; i<=axt->symCount; ++i)
    {
    int advanceQ = (isalpha(axt->qSym[i]) ? 1 : 0);
    int advanceT = (isalpha(axt->tSym[i]) ? 1 : 0);
    thisIn = (advanceQ && advanceT);
    if (thisIn)
        {
	if (!lastIn)
	    {
	    qStart = qPos;
	    tStart = tPos;
	    }
	}
    else
        {
	if (lastIn)
	    {
	    int size = qPos - qStart;
	    assert(size == tPos - tStart);
	    if (size > 0)
	        {
		struct boxIn *b;
		AllocVar(b);
		b->qStart = qStart;
		b->qEnd = qPos;
		b->tStart = tStart;
		b->tEnd = tPos;
		slAddHead(pList, b);
		}
	    }
	}
    lastIn = thisIn;
    qPos += advanceQ;
    tPos += advanceT;
    }
}

void loadIfNewSeq(char *nibDir, char *newName, char strand, 
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* Load sequence unless it is already loaded.  Reverse complement
 * if necessary. */
{
struct dnaSeq *seq;
if (sameString(newName, *pName))
    {
    if (strand != *pStrand)
        {
	seq = *pSeq;
	reverseComplement(seq->dna, seq->size);
	*pStrand = strand;
	}
    }
else
    {
    char fileName[512];
    freeDnaSeq(pSeq);
    snprintf(fileName, sizeof(fileName), "%s/%s.nib", nibDir, newName);
    *pName = newName;
    *pSeq = seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    uglyf("Loaded %d bases in %s\n", seq->size, fileName);
    }
}

int boxInCmpBoth(const void *va, const void *vb)
/* Compare to sort based on query, then target. */
{
const struct boxIn *a = *((struct boxIn **)va);
const struct boxIn *b = *((struct boxIn **)vb);
int dif;
dif = a->qStart - b->qStart;
if (dif == 0)
    dif = a->tStart - b->tStart;
return dif;
}

void removeExactOverlaps(struct boxIn **pBoxList)
/* Remove from list blocks that start in exactly the same
 * place on both coordinates. */
{
struct boxIn *newList = NULL, *b, *next, *last = NULL;
slSort(pBoxList, boxInCmpBoth);
for (b = *pBoxList; b != NULL; b = next)
    {
    next = b->next;
    if (last != NULL && b->qStart == last->qStart && b->tStart == last->tStart)
        {
	/* Fold this block into previous one. */
	if (last->qEnd < b->qEnd) last->qEnd = b->qEnd;
	if (last->tEnd < b->tEnd) last->tEnd = b->tEnd;
	freeMem(b);
	}
    else
	{
	slAddHead(&newList, b);
	last = b;
	}
    }
slReverse(&newList);
*pBoxList = newList;
}

int scoreBlock(char *q, char *t, int size, int matrix[256][256])
/* Score block through matrix. */
{
int score = 0;
int i;
for (i=0; i<size; ++i)
    score += matrix[q[i]][t[i]];
return score;
}

static void findCrossover(struct boxIn *left, struct boxIn *right,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq,  
	int overlap, int matrix[256][256], int *retPos, int *retScoreAdjustment)
/* Find ideal crossover point of overlapping blocks.  That is
 * the point where we should start using the right block rather
 * than the left block.  This point is an offset from the start
 * of the overlapping region (which is the same as the start of the
 * right block). */
{
int bestPos = 0;
char *rqStart = qSeq->dna + right->qStart;
char *lqStart = qSeq->dna + left->qEnd - overlap;
char *rtStart = tSeq->dna + right->tStart;
char *ltStart = tSeq->dna + left->tEnd - overlap;
int i;
int score, bestScore, rScore, lScore;

score = bestScore = rScore = scoreBlock(rqStart, rtStart, overlap, matrix);
lScore = scoreBlock(lqStart, ltStart, overlap, matrix);
for (i=0; i<overlap; ++i)
    {
    score += matrix[lqStart[i]][ltStart[i]];
    score -= matrix[rqStart[i]][rtStart[i]];
    if (score > bestScore)
	{
	bestScore = score;
	bestPos = i+1;
	}
    }
*retPos = bestPos;
*retScoreAdjustment = rScore + lScore - bestScore;
}

struct scoreData
/* Data needed to score block. */
    {
    struct dnaSeq *qSeq;	/* Query sequence. */
    struct dnaSeq *tSeq;	/* Target sequence. */
    struct axtScoreScheme *ss;  /* Scoring scheme. */
    double gapPower;		/* Power to raise gap size to. */
    };
struct scoreData scoreData;

int interpolate(int x, int *s, int *v, int sCount)
/* Find closest value to x in s, and then lookup corresponding
 * value in v.  Interpolate where necessary. */
{
int i, ds, dv, ss;
for (i=0; i<sCount; ++i)
    {
    ss = s[i];
    if (x == ss)
        return v[i];
    else if (x < ss)
        {
	ds = ss - s[i-1];
	dv = v[i] - v[i-1];
	return v[i-1] + dv * (x - s[i-1]) / ds;
	}
    }
/* If get to here extrapolate from last two values */
ds = s[sCount-1] - s[sCount-2];
dv = v[sCount-1] - v[sCount-2];
return v[sCount-2] + dv * (x - s[sCount-2]) / ds;
}

/* Tables that define piecewise linear gap costs. */
static int gapInitPos[29] = { 1,2,3,4,5,6,7,8,9,10,15,20,30,40,
	60,80,100,150,200,300,400,500,1000,2000,5000,10000,20000,
	50000,100000,};
static int gapInitQGap[24] = { -397,-454,-478,-497,-532,-558,-578,
	-595,-611,-626,-698,-752,-833,-899,-989,-1027,-1033,-1052,
	-1054,-1155,-1147,-1224,-1344,-1450,};
static int gapInitTGap[25] = { -359,-412,-443,-467,-492,-512,-528,
	-540,-555,-565,-619,-669,-763,-842,-956,-1022,-1078,-1148,
	-1177,-1037,-1192,-1253,-1344,-1426,-1630,};
static int gapInitBothGap[29] = { 0,-1186,-1186,-1137,-1186,-1137,
	-1186,-1137,-1038,-955,-886, -840,-826,-829,-868,-891,-914,
	-944,-949,-966,-972,-1004,-1069,-1138, -1223,-1312,-1403,
	-1584,-1765,};

int oldCost(int dq, int dt)
/* Figure out gap costs. */
{
if (dq == 0)
    return interpolate(dt, gapInitPos, gapInitTGap, ArraySize(gapInitTGap));
else if (dt == 0)
    return interpolate(dq, gapInitPos, gapInitQGap, ArraySize(gapInitQGap));
else
    {
    int both = dq + dt;
    return interpolate(both, gapInitPos, gapInitBothGap, ArraySize(gapInitBothGap));
    }
}

struct gapAid
/* A structure that bundles together stuff to help us
 * calculate gap costs quickly. */
    {
    int smallSize;	/* Size of tables for doing quick lookup of small gaps. */
    int *qSmall;    /* Table for small gaps in q; */
    int *tSmall;	/* Table for small gaps in t. */
    int *bSmall;    /* Table for small gaps in either. */
    int *longPos;/* Table of positions to interpolate between for larger gaps. */
    int *qLong;	/* Values to interpolate between for larger gaps in q. */
    int *tLong;	/* Values to interpolate between for larger gaps in t. */
    int *bLong;	/* Values to interpolate between for larger gaps in both. */
    int longCount;	/* Number of long positions overall in longPos. */
    int qPosCount;	/* Number of long positions in q. */
    int tPosCount;	/* Number of long positions in t. */
    int bPosCount;	/* Number of long positions in b. */
    } aid;

void initGapAid()
/* Initialize gap aid structure for faster gap
 * computations. */
{
int i, startLong = -1;
aid.smallSize = 100;
AllocArray(aid.qSmall, aid.smallSize);
AllocArray(aid.tSmall, aid.smallSize);
AllocArray(aid.bSmall, aid.smallSize);
for (i=1; i<100; ++i)
    {
    aid.qSmall[i] = interpolate(i, gapInitPos, gapInitQGap, ArraySize(gapInitQGap));
    aid.tSmall[i] = interpolate(i, gapInitPos, gapInitTGap, ArraySize(gapInitTGap));
    aid.bSmall[i] = interpolate(i, gapInitPos, 
    	gapInitBothGap, ArraySize(gapInitBothGap));
    }
for (i=0; i<ArraySize(gapInitPos); ++i)
    {
    if (aid.smallSize == gapInitPos[i])
	{
        startLong = i;
	break;
	}
    }
if (startLong < 0)
    errAbort("No position %d in initGapAid()\n", aid.smallSize);
aid.longCount = ArraySize(gapInitPos) - startLong;
aid.qPosCount = ArraySize(gapInitQGap) - startLong;
aid.tPosCount = ArraySize(gapInitTGap) - startLong;
aid.bPosCount = ArraySize(gapInitBothGap) - startLong;
aid.longPos = cloneMem(gapInitPos + startLong, aid.longCount * sizeof(int));
aid.qLong = cloneMem(gapInitQGap + startLong, aid.qPosCount * sizeof(int));
aid.tLong = cloneMem(gapInitTGap + startLong, aid.tPosCount * sizeof(int));
aid.bLong = cloneMem(gapInitBothGap + startLong, aid.bPosCount * sizeof(int));

uglyf("longPos (%d): ", aid.longCount);
for (i=0; i<aid.longCount; ++i)
    uglyf("%d,", aid.longPos[i]);
uglyf("\n");
uglyf("qLong (%d): ", aid.qPosCount);
for (i=0; i<aid.qPosCount; ++i)
    uglyf("%d,", aid.qLong[i]);
uglyf("\n");
uglyf("tLong (%d): ", aid.tPosCount);
for (i=0; i<aid.tPosCount; ++i)
    uglyf("%d,", aid.tLong[i]);
uglyf("\n");
uglyf("bLong (%d): ", aid.bPosCount);
for (i=0; i<aid.bPosCount; ++i)
    uglyf("%d,", aid.bLong[i]);
uglyf("\n");
}

int gapCost(int dq, int dt)
/* Figure out gap costs. */
{
if (dt == 0)
    {
    if (dq < aid.smallSize)
        return aid.qSmall[dq];
    else
        return interpolate(dq, aid.longPos, aid.qLong, aid.qPosCount);
    }
else if (dq == 0)
    {
    if (dt < aid.smallSize)
        return aid.tSmall[dt];
    else
        return interpolate(dt, aid.longPos, aid.tLong, aid.tPosCount);
    }
else
    {
    int both = dq + dt;
    if (both < aid.smallSize)
        return aid.bSmall[both];
    else
        return interpolate(both, aid.longPos, aid.bLong, aid.bPosCount);
    }
}

void testGaps()
{
int i, g;
for (i=1; i<ArraySize(gapInitPos); ++i)
    {
    g = (gapInitPos[i-1] + gapInitPos[i])/2;
    printf("%d: %d %d %d\n", g, gapCost(g,0), gapCost(0,g), gapCost(g/2,g-g/2));
    printf("%d: %d %d %d\n", g, oldCost(g,0), oldCost(0,g), oldCost(g/2,g-g/2));
    }
g = 200000;
printf("%d: %d %d %d\n", g, gapCost(g,0), gapCost(0,g), gapCost(g/2,g-g/2));
g = 1000000;
printf("%d: %d %d %d\n", g, gapCost(g,0), gapCost(0,g), gapCost(g/2,g-g/2));
uglyAbort("All for now");
}

static int connCount = 0;
static int overlapCount = 0;

int connectCost(struct boxIn *a, struct boxIn *b)
/* Calculate connection cost - including gap score
 * and overlap adjustments if any. */
{
int dq = b->qStart - a->qEnd;
int dt = b->tStart - a->tEnd;
int overlapAdjustment = 0;
int gap;
if (dq < 0 || dt < 0)
   {
   int overlap = -min(dq, dt);
   int crossover;
   findCrossover(a, b, scoreData.qSeq, scoreData.tSeq, overlap, 
   	scoreData.ss->matrix,
   	&crossover, &overlapAdjustment);
   dq += overlap;
   dt += overlap;
   ++overlapCount;
   }
gap = gapCost(dq, dt);
uglyf("%d %d cost %d\n", dq, dt, gap);
++connCount;
return overlapAdjustment - gap;
// return 400 * pow(dt+dq, scoreData.gapPower) + overlapAdjustment;
}

void calcChainBounds(struct chain *chain)
/* Recalculate chain boundaries. */
{
struct boxIn *b = chain->blockList;
chain->qStart = b->qStart;
chain->tStart = b->tStart;
b = slLastEl(chain->blockList);
chain->qEnd = b->qEnd;
chain->tEnd = b->tEnd;
}

boolean removeNegativeBlocks(struct chain *chain)
/* Removing the partial overlaps occassional results
 * in all of a block being removed.  This routine
 * removes the dried up husks of these blocks 
 * and returns TRUE if it finds any. */
{
struct boxIn *newList = NULL, *b, *next;
boolean gotNeg = FALSE;
for (b = chain->blockList; b != NULL; b = next)
    {
    next = b->next;
    if (b->qStart >= b->qEnd || b->tStart >= b->tEnd)
	{
        gotNeg = TRUE;
	freeMem(b);
	}
    else
        {
	slAddHead(&newList, b);
	}
    }
slReverse(&newList);
chain->blockList = newList;
if (gotNeg)
    calcChainBounds(chain);
return gotNeg;
}

void removePartialOverlaps(struct chain *chain, 
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, int matrix[256][256])
/* If adjacent blocks overlap then find crossover points between them. */
{
struct boxIn *b, *nextB;

do
    {
    for (b = chain->blockList; b != NULL; b = nextB)
	{
	nextB = b->next;
	if (nextB != NULL)
	    {
	    int dq = nextB->qStart - b->qEnd;
	    int dt = nextB->tStart - b->tEnd;
	    if (dq < 0 || dt < 0)
	       {
	       int overlap = -min(dq, dt);
	       int crossover, invCross, overlapAdjustment;
	       findCrossover(b, nextB, scoreData.qSeq, scoreData.tSeq, overlap, 
		    scoreData.ss->matrix,
		    &crossover, &overlapAdjustment);
	       nextB->qStart += crossover;
	       nextB->tStart += crossover;
	       invCross = overlap - crossover;
	       b->qEnd -= invCross;
	       b->tEnd -= invCross;
	       }
	    }
	}
    }
while (removeNegativeBlocks(chain));
}

void chainPair(struct seqPair *sp,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct chain **pChainList)
/* Chain up blocks and output. */
{
struct chain *chainList, *chain, *next;
struct boxIn *b;
long startTime, dt;

uglyf("chainPair %s\n", sp->name);

/* Set up info for connect function. */
scoreData.qSeq = qSeq;
scoreData.tSeq = tSeq;
scoreData.ss = axtScoreSchemeDefault();
scoreData.gapPower = 1.0/2.5;

/* Score blocks. */
for (b = sp->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    b->score = axtScoreUngapped(scoreData.ss, 
    	qSeq->dna + b->qStart, tSeq->dna + b->tStart, size);
    }


/* Get chain list and clean it up a little. */
startTime = clock1000();
chainList = chainBlocks(sp->qName, qSeq->size, sp->qStrand, 
	sp->tName, tSeq->size, &sp->blockList, connectCost);
dt = clock1000() - startTime;
uglyf("Made %d chains in %5.3f s\n", slCount(chainList), dt*0.001);
for (chain = chainList; chain != NULL; chain = chain->next)
    removePartialOverlaps(chain, qSeq, tSeq, scoreData.ss->matrix);

/* Move chains scoring over threshold to master list. */
for (chain = chainList; chain != NULL; chain = next)
    {
    next = chain->next;
    if (chain->score >= minScore)
        {
	slAddHead(pChainList, chain);
	}
    else 
        {
	chainFree(&chain);
	}
    }
}

void axtChain(char *axtIn, char *tNibDir, char *qNibDir, char *chainOut)
/* axtChain - Chain together axt alignments.. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct dyString *dy = newDyString(512);
struct lineFile *lf = lineFileOpen(axtIn, TRUE);
struct axt *axt;
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(chainOut, "w");
char *qName = "",  *tName = "";
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char qStrand = 0, tStrand = 0;
struct chain *chainList = NULL, *chain;

/* Read input file and divide alignments into various parts. */
while ((axt = axtRead(lf)) != NULL)
    {
    struct boxIn *block;
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(axt->qName);
	sp->tName = cloneString(axt->tName);
	sp->qStrand = axt->qStrand;
	}
    addAxtBlocks(&sp->blockList, axt);
    sp->axtCount += 1;
    axtFree(&axt);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->blockList);
    uglyf("Got %d blocks, %d axts in %s\n", slCount(sp->blockList), sp->axtCount, sp->name);
    removeExactOverlaps(&sp->blockList);
    uglyf("%d blocks after duplicate removal\n", slCount(sp->blockList));
    loadIfNewSeq(qNibDir, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
    loadIfNewSeq(tNibDir, sp->tName, '+', &tName, &tSeq, &tStrand);
    chainPair(sp, qSeq, tSeq, &chainList);
    }
slSort(&chainList, chainCmpScore);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    struct boxIn *b, *nextB;
    assert(chain->qStart == chain->blockList->qStart 
    	&& chain->tStart == chain->blockList->tStart);
    fprintf(f, "chain %d %s %d + %d %d %s %d %c %d %d\n", chain->score,
	chain->tName, chain->tSize, chain->tStart, chain->tEnd,
	chain->qName, chain->qSize, chain->qStrand, chain->qStart, chain->qEnd);
    for (b = chain->blockList; b != NULL; b = nextB)
	{
	nextB = b->next;
	fprintf(f, "%d", b->qEnd - b->qStart);
	if (nextB != NULL)
	    fprintf(f, "\t%d\t%d", nextB->tStart - b->tEnd, nextB->qStart - b->qEnd);
	fputc('\n', f);
	}
    fputc('\n', f);
    }

dyStringFree(&dy);
uglyf("ConnCount = %d, overlapCount = %d\n", connCount, overlapCount);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
minScore = optionInt("minScore", minScore);
dnaUtilOpen();
initGapAid();
testGaps();
if (argc != 5)
    usage();
axtChain(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
