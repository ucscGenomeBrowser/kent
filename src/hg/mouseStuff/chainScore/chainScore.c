/* chainScore - score chains. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"
#include "axt.h"
#include "psl.h"
#include "boxClump.h"
#include "chainBlock.h"
#include "portable.h"

int minScore = 1000;
char *detailsName = NULL;
char *gapFileName = NULL;

struct scoreData
/* Data needed to score block. */
    {
    struct dnaSeq *qSeq;	/* Query sequence. */
    struct dnaSeq *tSeq;	/* Target sequence. */
    struct axtScoreScheme *ss;  /* Scoring scheme. */
    double gapPower;		/* Power to raise gap size to. */
    };
struct scoreData scoreData;
struct axtScoreScheme *scoreScheme = NULL;

int interpolate(int x, int *s, double *v, int sCount)
/* Find closest value to x in s, and then lookup corresponding
 * value in v.  Interpolate where necessary. */
{
int i, ds, ss;
double dv;
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

static int *gapInitPos ;  
static double *gapInitQGap ;  
static double *gapInitTGap ;  
static double *gapInitBothGap ;

static int gapInitPosDefault[] = { 
   1,   2,   3,   11,  111, 2111, 12111, 32111,  72111, 152111, 252111,
};
static double gapInitQGapDefault[] = { 
   350, 425, 450, 600, 900, 2900, 22900, 57900, 117900, 217900, 317900,
};
static double gapInitTGapDefault[] = { 
   350, 425, 450, 600, 900, 2900, 22900, 57900, 117900, 217900, 317900,
};
static double gapInitBothGapDefault[] = { 
   400+350, 400+425, 400+450, 400+600, 400+900, 400+2900, 
   400+22900, 400+57900, 400+117900, 400+217900, 400+317900,
};

struct gapAid
/* A structure that bundles together stuff to help us
 * calculate gap costs quickly. */
    {
    int smallSize; /* Size of tables for doing quick lookup of small gaps. */
    int *qSmall;   /* Table for small gaps in q; */
    int *tSmall;   /* Table for small gaps in t. */
    int *bSmall;   /* Table for small gaps in either. */
    int *longPos;/* Table of positions to interpolate between for larger gaps. */
    double *qLong; /* Values to interpolate between for larger gaps in q. */
    double *tLong; /* Values to interpolate between for larger gaps in t. */
    double *bLong; /* Values to interpolate between for larger gaps in both. */
    int longCount;	/* Number of long positions overall in longPos. */
    int qPosCount;	/* Number of long positions in q. */
    int tPosCount;	/* Number of long positions in t. */
    int bPosCount;	/* Number of long positions in b. */
    int qLastPos;	/* Maximum position we have data on in q. */
    int tLastPos;	/* Maximum position we have data on in t. */
    int bLastPos;	/* Maximum position we have data on in b. */
    double qLastPosVal;	/* Value at max pos. */
    double tLastPosVal;	/* Value at max pos. */
    double bLastPosVal;	/* Value at max pos. */
    double qLastSlope;	/* What to add for each base after last. */
    double tLastSlope;	/* What to add for each base after last. */
    double bLastSlope;	/* What to add for each base after last. */
    } aid;

struct seqPair
/* Pair of sequences. */
  {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct chain *chain; /* List of alignments. */
    };

int seqPairCmp(const void *va, const void *vb)
/* Compare to sort based on tName,qName. */
{
const struct seqPair *a = *((struct seqPair **)va);
const struct seqPair *b = *((struct seqPair **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = strcmp(a->qName, b->qName);
if (dif == 0)
    dif = (int)a->qStrand - (int)b->qStrand;
return dif;
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
void loadFaSeq(struct hash *faHash, char *newName, char strand, 
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* retrieve sequence from hash.  Reverse complement
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
    *pName = newName;
    *pSeq = seq = hashFindVal(faHash, newName);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    uglyf("Loaded %d bases from %s fa\n", seq->size, newName);
    }
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainScore - score chains\n"
  "usage:\n"
  "   chainScore in.chain tNibDir qNibDir out.chain\n"
  "options:\n"
  "   -minScore=N  Minimum score for chain, default %d\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file\n"
  "   -linearGap=filename Read piecewise linear gap from tab delimited file\n"
  "   sample linearGap file \n"
  "tablesize 11\n"
  "smallSize 111\n"
  "position 1 2 3 11 111 2111 12111 32111 72111 152111 252111\n"
  "qGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
  "tGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
  "bothGap 750 825 850 1000 1300 3300 23300 58300 118300 218300 318300\n"
  , minScore

  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int gapCost(int dq, int dt)
/* Figure out gap costs. */
{
if (dt < 0) dt = 0;
if (dq < 0) dq = 0;
if (dt == 0)
    { 
    if (dq < aid.smallSize)
        return aid.qSmall[dq];
    else if (dq >= aid.qLastPos)
        return aid.qLastPosVal + aid.qLastSlope * (dq-aid.qLastPos);
    else
        return interpolate(dq, aid.longPos, aid.qLong, aid.qPosCount);
    }
else if (dq == 0)
    {
    if (dt < aid.smallSize)
        return aid.tSmall[dt];
    else if (dt >= aid.tLastPos)
        return aid.tLastPosVal + aid.tLastSlope * (dt-aid.tLastPos);
    else
        return interpolate(dt, aid.longPos, aid.tLong, aid.tPosCount);
    }
else
    {
    int both = dq + dt;
    if (both < aid.smallSize)
        return aid.bSmall[both];
    else if (both >= aid.bLastPos)
        return aid.bLastPosVal + aid.bLastSlope * (both-aid.bLastPos);
    else
        return interpolate(both, aid.longPos, aid.bLong, aid.bPosCount);
    }
}

double scoreBlock(char *q, char *t, int size, int matrix[256][256])
/* Score block through matrix. */
{
double score = 0;
int i;
for (i=0; i<size; ++i)
    score += matrix[q[i]][t[i]];
return score;
}

double chainScore(struct chain *chain, struct dnaSeq *qSeq, struct dnaSeq *tSeq,
    int matrix[256][256], int (*gapCost)(int dt, int dq))
/* Calculate score of chain from scratch looking at blocks. */
{
struct cBlock *b, *a = NULL;
double score = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    score += scoreBlock(qSeq->dna + b->qStart, tSeq->dna + b->tStart, 
    	size, matrix);
    if (a != NULL)
	score -= gapCost(b->tStart - a->tEnd, b->qStart - a->qEnd);
    a = b;
    }
return score;
}

void scorePair(struct seqPair *sp,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct chain **pChainList,
	struct chain *chainList)
/* Chain up blocks and output. */
{
struct chain  *chain, *next;
struct cBlock *b;

/* Set up info for connect function. */
scoreData.qSeq = qSeq;
scoreData.tSeq = tSeq;
scoreData.ss = scoreScheme;
scoreData.gapPower = 1.0/2.5;

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    chain->score = chainScore(chain, qSeq, tSeq, scoreData.ss->matrix, gapCost);
    }

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

void doChainScore(char *chainIn, char *tNibDir, char *qNibDir, char *chainOut)
{
char qStrand = 0, tStrand = 0;
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char *qName = "",  *tName = "";
FILE *f = mustOpen(chainOut, "w");
struct chain *chainList = NULL, *chain;
struct chain *inputChains, *next;
FILE *details = NULL;
struct lineFile *lf = NULL;
struct dnaSeq *seq, *seqList = NULL;
struct hash *faHash = newHash(0);
struct hash *chainHash = newHash(0);
char comment[1024];
FILE *faF;
struct seqPair *spList = NULL, *sp;
struct dyString *dy = newDyString(512);
struct lineFile *chainsLf = lineFileOpen(chainIn, TRUE);

while ((chain = chainRead(chainsLf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", chain->qName, chain->qStrand, chain->tName);
    sp = hashFindVal(chainHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(chainHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(chain->qName);
	sp->tName = cloneString(chain->tName);
	sp->qStrand = chain->qStrand;
	}
    slAddHead(&sp->chain, chain);
    }
slSort(&spList, seqPairCmp);
lineFileClose(&chainsLf);

if (optionExists("faQ"))
    {
    faF = mustOpen(qNibDir, "r");
    while ( faReadMixedNext(faF, TRUE, NULL, TRUE, NULL, &seq))
        {
        hashAdd(faHash, seq->name, seq);
        slAddHead(&seqList, seq);
        }
    fclose(faF);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    if (optionExists("faQ"))
        {
        assert (faHash != NULL);
        loadFaSeq(faHash, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
        }
    else
        loadIfNewSeq(qNibDir, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
    loadIfNewSeq(tNibDir, sp->tName, '+', &tName, &tSeq, &tStrand);
    scorePair(sp, qSeq, tSeq, &chainList, sp->chain);
    }


slSort(&chainList, chainCmpScore);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    assert(chain->qStart == chain->blockList->qStart 
	&& chain->tStart == chain->blockList->tStart);
    chainWrite(chain, f);
    }

carefulClose(&f);
}

double calcSlope(double y2, double y1, double x2, double x1)
/* Calculate slope of line from x1/y1 to x2/y2 */
{
return (y2-y1)/(x2-x1);
}

void initGapAid(char *gapFileName)
/* Initialize gap aid structure for faster gap
 * computations. */
{
int i, tableSize, startLong = -1;
char *sizeDesc[2];
char *words[128];

if (gapFileName != NULL)
    {
    struct lineFile *lf = lineFileOpen(gapFileName, TRUE);
    int count;

    lineFileNextRowTab(lf, sizeDesc, 2);
    tableSize = atoi(sizeDesc[1]);
    AllocArray(gapInitPos,tableSize);
    AllocArray(gapInitQGap,tableSize);
    AllocArray(gapInitTGap,tableSize);
    AllocArray(gapInitBothGap,tableSize);
    while (count = lineFileChopNext(lf, words, tableSize+1))
        {
        if (sameString(words[0],"smallSize"))
            {
            aid.smallSize = atoi(words[1]);
            }
        if (sameString(words[0],"position"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitPos[i] = atoi(words[i+1]);
            }
        if (sameString(words[0],"qGap"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitQGap[i] = atoi(words[i+1]);
            }
        if (sameString(words[0],"tGap"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitTGap[i] = atoi(words[i+1]);
            }
        if (sameString(words[0],"bothGap"))
            {
            for (i=0 ; i<count-1 ; i++)
                gapInitBothGap[i] = atoi(words[i+1]);
            }
            
        }
    if (aid.smallSize == 0)
        errAbort("missing smallSize parameter in %s\n",gapFileName);
    lineFileClose(&lf);
    }
else
    {
    /* if no gap file, then setup default values */ 
    /* Set up to handle small values */
    aid.smallSize = 111;
    tableSize = 11;
    AllocArray(gapInitPos,tableSize);
    AllocArray(gapInitQGap,tableSize);
    AllocArray(gapInitTGap,tableSize);
    AllocArray(gapInitBothGap,tableSize);
    for (i = 0 ; i < tableSize ; i++)
        {
        gapInitPos[i] = gapInitPosDefault[i];
        gapInitTGap[i] = gapInitTGapDefault[i];
        gapInitQGap[i] = gapInitQGapDefault[i];
        gapInitBothGap[i] = gapInitBothGapDefault[i];
        }
    }
    AllocArray(aid.qSmall, aid.smallSize);
    AllocArray(aid.tSmall, aid.smallSize);
    AllocArray(aid.bSmall, aid.smallSize);
    for (i=1; i<aid.smallSize; ++i)
        {
        aid.qSmall[i] = 
            interpolate(i, gapInitPos, gapInitQGap, tableSize);
        aid.tSmall[i] = 
            interpolate(i, gapInitPos, gapInitTGap, tableSize);
        aid.bSmall[i] = interpolate(i, gapInitPos, 
            gapInitBothGap, tableSize);
        }

    /* Set up to handle intermediate values. */
    for (i=0; i<tableSize; ++i)
        {
        if (aid.smallSize == gapInitPos[i])
            {
            startLong = i;
            break;
            }
        }
    if (startLong < 0)
        errAbort("No position %d in initGapAid()\n", aid.smallSize);
    aid.longCount = tableSize - startLong;
    aid.qPosCount = tableSize - startLong;
    aid.tPosCount = tableSize - startLong;
    aid.bPosCount = tableSize - startLong;
    aid.longPos = cloneMem(gapInitPos + startLong, aid.longCount * sizeof(int));
    aid.qLong = cloneMem(gapInitQGap + startLong, aid.qPosCount * sizeof(double));
    aid.tLong = cloneMem(gapInitTGap + startLong, aid.tPosCount * sizeof(double));
    aid.bLong = cloneMem(gapInitBothGap + startLong, aid.bPosCount * sizeof(double));

    /* Set up to handle huge values. */
    aid.qLastPos = aid.longPos[aid.qPosCount-1];
    aid.tLastPos = aid.longPos[aid.tPosCount-1];
    aid.bLastPos = aid.longPos[aid.bPosCount-1];
    aid.qLastPosVal = aid.qLong[aid.qPosCount-1];
    aid.tLastPosVal = aid.tLong[aid.tPosCount-1];
    aid.bLastPosVal = aid.bLong[aid.bPosCount-1];
    aid.qLastSlope = calcSlope(aid.qLastPosVal, aid.qLong[aid.qPosCount-2],
                               aid.qLastPos, aid.longPos[aid.qPosCount-2]);
    aid.tLastSlope = calcSlope(aid.tLastPosVal, aid.tLong[aid.tPosCount-2],
                               aid.tLastPos, aid.longPos[aid.tPosCount-2]);
    aid.bLastSlope = calcSlope(aid.bLastPosVal, aid.bLong[aid.bPosCount-2],
                               aid.bLastPos, aid.longPos[aid.bPosCount-2]);
    // uglyf("qLastPos %d, qlastPosVal %f, qLastSlope %f\n", aid.qLastPos, aid.qLastPosVal, aid.qLastSlope);
    // uglyf("tLastPos %d, tlastPosVal %f, tLastSlope %f\n", aid.tLastPos, aid.tLastPosVal, aid.tLastSlope);
    // uglyf("bLastPos %d, blastPosVal %f, bLastSlope %f\n", aid.bLastPos, aid.bLastPosVal, aid.bLastSlope);
}

int main(int argc, char *argv[])
{
char *scoreSchemeName = NULL;
optionHash(&argc, argv);
minScore = optionInt("minScore", minScore);
detailsName = optionVal("details", NULL);
gapFileName = optionVal("linearGap", NULL);
scoreSchemeName = optionVal("scoreScheme", NULL);
if (scoreSchemeName != NULL)
    {
    printf("Reading scoring matrix from %s\n", scoreSchemeName);
    scoreScheme = axtScoreSchemeRead(scoreSchemeName);
    }
else
    scoreScheme = axtScoreSchemeDefault();
dnaUtilOpen();
initGapAid(gapFileName);
// testGaps();
if (argc != 5)
    usage();
doChainScore(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
