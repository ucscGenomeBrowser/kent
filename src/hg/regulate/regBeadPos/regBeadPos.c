/* regBeadPos - Position regulatory beads along a chromosome string.  The beads are
 * nucleosomes, open regions and closed regions.  */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigWig.h"
#include "hmmstats.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

char *simpleBed = NULL;
FILE *simpleBedFile = NULL;
char *stateProb = NULL;
FILE *stateProbFile = NULL;
double peakThreshold=40;
double minPeakSum = 500;
char *narrowPeak = NULL;
FILE *narrowPeakFile = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regBeadPos - Position regulatory beads along a chromosome string.  The beads\n"
  "are nucleosomes, open regions and closed regions.\n"
  "usage:\n"
  "   regBeadPos in.tab outFile.bed\n"
  "options:\n"
  "   -simpleBed=simpleOut.bed - output each little window separately\n"
  "   -narrowPeak=peaks.narrowPeak - output just the peak base here\n"
  "   -peakThreshold=N.N - min average dnase score for peak output\n"
  "   -stateProb=stateProbs.c\n"
  );
}


static struct optionSpec options[] = {
   {"simpleBed", OPTION_STRING},
   {"stateProb", OPTION_STRING},
   {"narrowPeak", OPTION_STRING},
   {"peakThreshold", OPTION_DOUBLE},
   {NULL, 0},
};

enum aStates 
/* Internal states for HMM. */
{
    aLow,                        /* Not much DNAse or histone activity. */
    aHisStart, aDnase, aHisEnd,
    aStateCount,
};

#define DNASE_LEVELS 5
#define HISTONE_LEVELS 5
#define HMM_LETTERS (DNASE_LEVELS * HISTONE_LEVELS)

char visStates[] =
/* User visible states of HMM. */
{
    '.',
    'O', 'x', 'O',
};

struct stateSummary
/* Keep track of letters seen in each state */
    {
    int counts[aStateCount][HMM_LETTERS];
    };
struct stateSummary stateSummary;

struct inFile
/* Info about input files. */
    {
    struct inFile *next;
    char *name;		/* Symbolic name used inside program - DNASE or HISTONE */
    char *fileName;	/* Full path name. */
    struct bbiFile *bigWig;			/* Open big wig */
    struct bigWigValsOnChrom *chromVals;	/* Fast bigWig access */
    double cuts[5];	/* Where we make the cuts on different levels */
    };

struct hash *makeInFilesHash(char *fileName)
/* Read input and make hash out of it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[7];
struct hash *hash = hashNew(0);
while (lineFileRow(lf, row))
    {
    struct inFile *in;
    AllocVar(in);
    in->name = cloneString(row[0]);
    in->fileName = cloneString(row[1]);
    in->bigWig = bigWigFileOpen(in->fileName);
    in->chromVals = bigWigValsOnChromNew();
    int i;
    for (i=0; i<ArraySize(in->cuts); ++i)
        in->cuts[i] = lineFileNeedDouble(lf, row, i+2);
    hashAdd(hash, in->name, in);
    }
return hash;
}

void *findInHashFromFile(char *key, struct hash *hash, char *fileName)
/* Like hashMustFindVal, but with a better error message. */
{
void *val = hashFindVal(hash, key);
if (val == NULL)
    errAbort("Can't find %s in %s\n", key, fileName);
return val;
}

inline UBYTE quant(struct inFile *inFile, double inVal)
/* Quantize inVal into something small using inFile->cuts */
{
double *cuts = inFile->cuts;

/* COuld do this if stack with a loop and a separate case for the
 * last value, but actually would take as about as many lines of code and be 
 * slower.  The code is not too slow because the first test handles 90% of
 * the cases in any case.... */
if (inVal < cuts[0])
    return 0;
else if (inVal < cuts[1])
    return 1;
else if (inVal < cuts[2])
    return 2;
else if (inVal < cuts[3])
    return 3;
else
    return 4;
}

void makeInLetters(int chromSize, struct inFile *a, struct inFile *b, int basesInWindow, 
	int *retLetterCount, UBYTE **retLetters)
/* This does two things - averages over a small window, and then quantizes the
 * result into one of a small number of values. The quantization is done in
 * a and b separately, and the letter produced in the end is aBin * binCount + bBin */
{
int letterCount = chromSize/basesInWindow;
UBYTE *letters;
AllocArray(letters, letterCount);
int i, startWin = 0;
double *aVals = a->chromVals->valBuf;
double *bVals = b->chromVals->valBuf;
double invWinSize = 1.0/basesInWindow;
for (i=0; i<letterCount; ++i)
    {
    int endWin = startWin + basesInWindow;
    int j;
    double aSum = 0, bSum = 0;
    for (j=startWin; j<endWin; ++j)
        {
	aSum += aVals[j];
	bSum += bVals[j];
	}
    double aMean = aSum*invWinSize;
    double bMean = bSum*invWinSize;
    UBYTE aQuant = quant(a, aMean), bQuant = quant(b, bMean);
    UBYTE letter = aQuant * DNASE_LEVELS + bQuant;
    letters[i] = letter;
    startWin = endWin;
    }
*retLetterCount = letterCount;
*retLetters = letters;
}

void dumpLetters(FILE *f, UBYTE *in, int count)
/* Put out given number of letters (which we obtain by adding 'a' to the input) */
{
int i;
for (i=0; i<count; ++i)
    {
    char c = in[i] + 'a';
    fputc(c, f);
    }
}

typedef UBYTE State;    /* A state. We'll only have 256 at most for now. */

/* This lets us keep the last two columns of scores. */
static double *scoreColumns[2];
static double *curScores, *prevScores;
static int scoreFlopper = 0; /* Flops between 0 and 1. */

static void flopScores()
/* Advance to next column. */
{
scoreFlopper = 1-scoreFlopper;
curScores = scoreColumns[scoreFlopper];
prevScores = scoreColumns[1-scoreFlopper];
}

static void initScores()
/* Allocate and initialize score columns. */
{
int i;

for (i=0; i<2; ++i)
    scoreColumns[i] = needMem(aStateCount * sizeof(scoreColumns[0][0]) );
flopScores();
}

#define unlikelyProb (1.0E-20)

/* Transition probabilities. */
static double *transProbLookup[256];

static void makeTransitionProbs()
/* Allocate transition probabilities and initialize them. */
{
int i, j;
double unlikely = log(unlikelyProb);

/* In this loop allocate transition table and set all values to
 * something improbable, but not so improbable that the log value
 * will overflow. */
for (i=0; i<aStateCount; ++i)
    {
    transProbLookup[i] = needMem(aStateCount * sizeof(transProbLookup[i][0]) );
    for (j=0; j<aStateCount; ++j)
        transProbLookup[i][j] = unlikely;
    }


transProbLookup[aLow][aLow] =     log(0.9999);
transProbLookup[aLow][aHisStart] =    log(0.0001);

transProbLookup[aHisStart][aHisStart] = log(0.99);
transProbLookup[aHisStart][aDnase] = log(0.01);

transProbLookup[aDnase][aDnase] = log(0.99);
transProbLookup[aDnase][aHisEnd] = log(0.01);

transProbLookup[aHisEnd][aHisEnd] = log(0.99);
transProbLookup[aHisEnd][aLow] = log(0.01);
}


double *probsFromDnaseHistones(double *dnase, double *histones)
{
double *p;
AllocArray(p, HMM_LETTERS);
int i,j, ix=0;
for (i=0; i<DNASE_LEVELS; ++i)
    for (j=0; j<HISTONE_LEVELS; ++j)
        {
	p[ix] = log(dnase[i]*histones[j]);
	++ix;
	}
return p;
}

double *makeEmissionProbsForLo()
{
static double dnase[5] = {0.93, 0.05, 0.019, 0.001, unlikelyProb};
static double histone[5] = {0.90, 0.08, 0.019, 0.001, unlikelyProb};
return probsFromDnaseHistones(dnase, histone);
}

double *makeEmissionProbsForDnase()
{
static double dnase[5] = {0.05, 0.10, 0.23, 0.30, 0.32};
static double histone[5] = {0.20, 0.20, 0.20, 0.20, 0.20};
return probsFromDnaseHistones(dnase, histone);
}

double *makeEmissionProbsForDnasePeak()
{
static double dnase[5] = {0.0001, 0.0001, 0.0998, 0.35, 0.55};
static double histone[5] = {0.20, 0.20, 0.20, 0.20, 0.20};
return probsFromDnaseHistones(dnase, histone);
}

double *makeEmissionProbsForFramingHistones()
{
static double dnase[5] = {0.27, 0.27, 0.27, 0.13, 0.1};
static double histone[5] = {0.07, 0.12, 0.24, 0.28, 0.29};
return probsFromDnaseHistones(dnase, histone);
}

double *lowProbs, *peakDnaseProbs, *highDnaseProbs, *framingHistoneProbs;

void makeEmissionProbs()
{
lowProbs = makeEmissionProbsForLo();
highDnaseProbs = makeEmissionProbsForDnase();
peakDnaseProbs = makeEmissionProbsForDnasePeak();
framingHistoneProbs = makeEmissionProbsForFramingHistones();
}

double *probsFromCounts(int *counts, int numSlots)
/* Sum up counts of slots and turn into probabilities
 * to see a value in that slot.  Uses a pseudocount
 * of 1 to avoid zero probabilities. Output is in form
 * of scaled logs. */
{
int i;
int pseudoCount = 1;
long long sum = 0;
for (i=0; i<numSlots; ++i)
    sum += counts[i] + pseudoCount;
double invSum = 1.0/sum;
double *p;
AllocArray(p, numSlots);
for (i=0; i<numSlots; ++i)
    {
    int count = counts[i] + pseudoCount;
    p[i] = log(count*invSum);
    }
return p;
}

void sumCounts(int size, int *a, int *b, int *out)
/* Add a and b of given size into out. */
{
int i;
for (i=0; i<size; ++i)
   out[i] = a[i] + b[i];
}

#ifdef SOMEDAY
void makeEmissionProbs()
{
static int aLowCounts[] = {11889074,215103,41151,26670,11037,111204,36430,20841,23551,17678,15675,8146,6616,10394,12824,9413,6322,4640,9454,15055,4614,5223,5066,8210,12701,};
static int aHDH1Counts[] = {3,953,3492,2766,1116,190,1584,2349,2744,2050,0,65,395,1061,1286,0,5,81,117,1440,0,0,0,18,139,};
static int aHDH2Counts[] = {0,13,40,7,0,839,6481,61,12,9,5373,2487,1172,131,35,4378,2060,1182,2002,744,1822,1922,1431,1810,2518,};
static int aHDH3Counts[] = {4,1011,4647,2600,582,202,1385,2328,2671,1150,3,19,216,1000,772,0,1,52,131,802,0,0,0,16,37,};

lowProbs = probsFromCounts(aLowCounts, HMM_LETTERS);
highDnaseProbs = probsFromCounts(aHDH2Counts, HMM_LETTERS);
int summedCounts[HMM_LETTERS];
sumCounts(HMM_LETTERS, aHDH1Counts, aHDH3Counts, summedCounts);
framingHistoneProbs = probsFromCounts(summedCounts, HMM_LETTERS);
}
#endif /* SOMEDAY */

#define startState(curState) \
    { \
    int destState = curState; \
    double newScore = -0x3fffffff; \
    State parent = 0; \
    double oneScore; 

#define endState(curState) \
    curScores[destState] = newScore; \
    allStates[destState][lettersIx] = parent; \
    }

#define source(sourceState, emitScore) \
    if ((oneScore = transProbLookup[sourceState][destState] + emitScore + prevScores[sourceState]) > newScore) \
        { \
        newScore = oneScore; \
        parent = sourceState; \
        }

#define prob1(table, letter) ((table)[letter])

void traceback(double *scores, State **allStates, int letterCount, State *tStates)
/* Fill in tStates with the states of along the optimal path */
{
int i;
double maxScore = -0x3fffffff;
int maxState = 0;
State *states;

/* Find end state. */
for (i=0; i<aStateCount; ++i)
    {
    if (scores[i] > maxScore)
        {
        maxScore = scores[i];
        maxState = i;
        }
    }

/* Fill in tStates with record of states of optimal path */
for (i = letterCount-1; i >= 0; i -= 1)
    {
    tStates[i] = maxState;
    states = allStates[maxState];
    maxState = states[i];
    }
}


void dynamo(UBYTE *letters, int letterCount, State *out)
/* Run dynamic program of HMM and put states of most likely path in out. */
{
State **allStates;
UBYTE *pos, c;
int stateCount = aStateCount;
int stateByteSize = letterCount * sizeof(State);
int i;
int lettersIx;
int scanSize = letterCount;
double unlikely = log(unlikelyProb);

/* Allocate state tables. */
allStates = needMem(stateCount * sizeof(allStates[0]));
for (i=0; i<stateCount; ++i)
    {
    allStates[i] = needLargeMem(stateByteSize);
    memset(allStates[i], 0, stateByteSize);
    }

/* Initialize score columns, and set up transitions from start state. */
initScores();
for (i=0; i<stateCount; ++i)
    prevScores[i] = unlikely;
prevScores[aLow] = log(0.9999);

for (lettersIx=0; lettersIx<scanSize; lettersIx += 1)
    {
    pos = letters+lettersIx;
    c = pos[0];

        
    startState(aLow)
        double b = prob1(lowProbs, c);
	source(aLow, b);
	source( aHisEnd, b);
    endState(aLow)
        
    startState(aHisStart)
        double b = prob1(framingHistoneProbs, c);
	source(aHisStart, b);
	source(aLow, b);
    endState(aHisStart)
        
    startState(aDnase)
        double b = prob1(highDnaseProbs, c);
	source(aDnase, b);
	source(aHisStart, b);
    endState(aDnase)
        
    startState(aHisEnd)
        double b = prob1(framingHistoneProbs, c);
	source(aHisEnd, b);
	source(aDnase, b);
    endState(aHisEnd)
        
    flopScores();
    }

traceback(prevScores, allStates, scanSize, out);

/* Clean up. */
for (i=0; i<stateCount; ++i)
    {
    freeMem(allStates[i]);
    }
freeMem(allStates);
}

void addStateCounts(char *chrom, UBYTE *letters,
	State *states, int letterCount, struct stateSummary *summary)
/* Add counts to summary.  */
{
int i;
for (i=0; i<letterCount; ++i)
    summary->counts[states[i]][letters[i]] += 1;
}

void writeOneStateSummaryInC(FILE *f, struct stateSummary *summary, char *name, State state)
{
fprintf(f, "double %s[] = {", name);
int i;
for (i=0; i<HMM_LETTERS; ++i)
    {
    fprintf(f, "%d,", summary->counts[state][i]);
    }
fprintf(f, "};\n");
}

void writeStateSummaryInC(FILE *f, struct stateSummary *summary)
/* Output summary as a C array */
{
writeOneStateSummaryInC(f, summary, "aLowCounts", aLow);
writeOneStateSummaryInC(f, summary, "aHisStart", aHisStart);
writeOneStateSummaryInC(f, summary, "aDnase", aDnase);
writeOneStateSummaryInC(f, summary, "aHisEnd", aHisEnd);
};

void outputSimpleStates(char *chrom, int shrinkFactor, State *states, int shrunkSize, FILE *f)
/* output bed - item every 5. */
{
/* Run it through a look up table. */
int i;
for (i=0; i<shrunkSize; ++i)
    {
    int start = i*shrinkFactor;
    char c = visStates[states[i]];
    if (c != '.')
	fprintf(f, "%s\t%d\t%d\t%c\n", chrom, start, start+shrinkFactor, visStates[states[i]]);
    }
}

void outputPeakIfOverThreshold(char *chrom, int chromStart, int chromEnd, 
	struct inFile *dnaseIn, int shrinkFactor, double threshold, FILE *f)
/* Output peak if over theshold and more than minimum size. */
{
int width = chromEnd - chromStart;
if (width > shrinkFactor)
    {
    double sum = 0;
    int i;
    double *valBuf = dnaseIn->chromVals->valBuf;
    double maxVal = valBuf[chromStart];
    int maxIx = chromStart;
    for (i=chromStart; i<chromEnd; ++i)
	{
	double val = valBuf[i];
	if (val > maxVal)
	    {
	    maxVal = val;
	    maxIx = i;
	    }
        sum += dnaseIn->chromVals->valBuf[i];
	}
    double ave = sum/width;
    if (ave >= threshold && sum >= minPeakSum)
        {
	int maxEndIx;
	for (maxEndIx=maxIx; maxEndIx<chromEnd; ++maxEndIx)
	    if (valBuf[maxEndIx] != maxVal)
	        break;
	int score = ave * 4;
	if (score > 1000)
	    score = 1000;
	fprintf(f, "%s\t%d\t%d\tpk\t%d\t.\t", chrom, maxIx, maxEndIx, score);
	fprintf(f, "%g\t-1\t-1\t%d\n", sum, (maxIx+maxEndIx)/2 - maxIx);
	}
    }
}

void newOutputStates(char *chrom, int shrinkFactor, State *states, int shrunkSize, FILE *f,
	struct inFile *dnaseIn, FILE *peakFile)
/* Convert runs of states to various types of BED lines. */
{
/* Run it through a look up table. */
int stateIx = 0;
char *labels = needMem(shrunkSize+1);	/* Extra so the countLeadingChars always end. */
int i;
for (i=0; i<shrunkSize; ++i)
    labels[i] = visStates[states[i]];

while (stateIx < shrunkSize)
    {
    char label = labels[stateIx];
    int n;
    int start = stateIx * shrinkFactor;
    switch (label)
        {
	case '.':
	    n = countLeadingChars(labels+stateIx, '.');
	    stateIx += n;
	    break;
	case 'O':
	    {
	    /* Count up something of form OOOxxxx^xxOO  - exactly one ^, else multiple */
	    char *pt = labels+stateIx;
	    int h1Count = countLeadingChars(pt, 'O');
	    pt += h1Count;
	    int dCount = countLeadingChars(pt, 'x');
	    pt += dCount;
	    int h2Count = countLeadingChars(pt, 'O');
	    pt += h2Count;
	    n = pt - (labels+stateIx);

	    int dnaseStart = start + (h1Count)*shrinkFactor;
	    int dnaseWidth = (dCount)*shrinkFactor;
	    int hisStart = start;
	    int hisWidth = (h1Count + dCount + h2Count)*shrinkFactor;
	    fprintf(f, "%s\t%d\t%d\t", chrom, hisStart, hisStart+hisWidth);
	    fprintf(f, "bead");
	    fprintf(f, "\t0\t.\t");
	    fprintf(f, "%d\t%d\n", dnaseStart,  dnaseStart + dnaseWidth);
	    stateIx += n;

	    if (peakFile != NULL)
	        {
		outputPeakIfOverThreshold(chrom, dnaseStart, dnaseStart + dnaseWidth, 
			dnaseIn, shrinkFactor, peakThreshold, peakFile);
		}
	    break;
	    }
	default:
	    internalErr();
	    break;
	}
    }
freez(&labels);
}

void oldOutputStates(char *chrom, int shrinkFactor, State *states, int shrunkSize, FILE *f)
/* Convert runs of states to various types of BED lines. */
{
/* Run it through a look up table. */
int stateIx = 0;
char *labels = needMem(shrunkSize+1);	/* Extra so the countLeadingChars always end. */
int i;
for (i=0; i<shrunkSize; ++i)
    labels[i] = visStates[states[i]];

while (stateIx < shrunkSize)
    {
    char label = labels[stateIx];
    int n;
    int dCount, hCount;
    int start = stateIx * shrinkFactor;
    switch (label)
        {
	case '.':
	    n = countLeadingChars(labels+stateIx, '.');
	    stateIx += n;
	    break;
	case 'm':
	    n = countLeadingChars(labels+stateIx, 'm');
	    fprintf(f, "%s\t%d\t%d\tmixed\n", chrom, start, start + n*shrinkFactor);
	    stateIx += n;
	    break;
	case '^':
	    dCount = countLeadingChars(labels+stateIx, '^');
	    hCount = countLeadingChars(labels+stateIx+dCount, 'o');
	    if (hCount <= 0)
	        {
		fprintf(f, "%s\t%d\t%d\tdnase\n", chrom, start, start + dCount*shrinkFactor);
		stateIx += dCount;
		}
	    else
	        {
		n = dCount + hCount;
		fprintf(f, "%s\t%d\t%d\tpair\n", chrom, start, start + n*shrinkFactor);
		stateIx += n;
		}
	    break;
	case 'o':
	    hCount = countLeadingChars(labels+stateIx, 'o');
	    dCount = countLeadingChars(labels+stateIx+hCount, '^');
	    if (dCount <= 0)
	        {
		fprintf(f, "%s\t%d\t%d\thistone\n", chrom, start, start + hCount*shrinkFactor);
		stateIx += hCount;
		}
	    else
	        {
		n = dCount + hCount;
		fprintf(f, "%s\t%d\t%d\tpair\n", chrom, start, start + n*shrinkFactor);
		stateIx += n;
		}
	    break;
	case 'O':
	    hCount = countLeadingChars(labels+stateIx, 'O');
	    n = hCount;
	    while ((dCount = countLeadingChars(labels+stateIx+n, '^')) > 0)
	        {
		n += dCount;
		n += countLeadingChars(labels+stateIx+n, 'O');
		}
	    fprintf(f, "%s\t%d\t%d\tframed\n", chrom, start, start + n*shrinkFactor);
	    stateIx += n;
	    break;
	default:
	    internalErr();
	    break;
	}
    }
freez(&labels);
}

void runHmmOnChrom(struct bbiChromInfo *chrom, struct inFile *dnaseIn, struct inFile *histoneIn, FILE *f)
/* Do the HMM run on the one chromosome. */
{
int inLetterCount;
UBYTE *inLetters;
makeInLetters(chrom->size, dnaseIn, histoneIn, 5, &inLetterCount, &inLetters);
uglyTime("Quantizing %s of size %d", chrom->name, chrom->size);
State *states;
AllocArray(states, inLetterCount);
dynamo(inLetters, inLetterCount, states);
uglyTime("Ran HMM dynamo");
newOutputStates(chrom->name, 5, states, inLetterCount, f, dnaseIn, narrowPeakFile);
if (simpleBedFile)
    outputSimpleStates(chrom->name, 5, states, inLetterCount, simpleBedFile);
if (stateProbFile)
    addStateCounts(chrom->name, inLetters, states, inLetterCount, &stateSummary);

uglyTime("Wrote output");
}


void regBeadPos(char *inTab, char *outFile)
/* regBeadPos - Position regulatory beads along a chromosome string.  The beads are nucleosomes, 
 * open regions and closed regions.. */
{
struct hash *inHash = makeInFilesHash(inTab);
uglyf("Read %d from %s\n", inHash->elCount, inTab);
struct inFile *dnaseIn = findInHashFromFile("DNASE", inHash, inTab);
struct inFile *histoneIn = findInHashFromFile("HISTONE", inHash, inTab);
uglyf("%s and %s found\n", dnaseIn->name, histoneIn->name);
FILE *f = mustOpen(outFile, "w");
if (simpleBed != NULL)
    simpleBedFile = mustOpen(simpleBed, "w");
if (stateProb != NULL)
    stateProbFile = mustOpen(stateProb, "w");
if (narrowPeak != NULL)
    narrowPeakFile = mustOpen(narrowPeak, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(dnaseIn->bigWig);
makeTransitionProbs();
makeEmissionProbs();
verbose(2, "Processing %d chromosomes\n", slCount(chromList));
uglyTime(NULL);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (bigWigValsOnChromFetchData(dnaseIn->chromVals, chrom->name, dnaseIn->bigWig)
    &&  bigWigValsOnChromFetchData(histoneIn->chromVals, chrom->name, histoneIn->bigWig))
        {
	uglyTime("Got data");
	runHmmOnChrom(chrom, dnaseIn, histoneIn, f);
	}
    }
if (stateProbFile)
    writeStateSummaryInC(stateProbFile, &stateSummary);
carefulClose(&stateProbFile);
carefulClose(&simpleBedFile);
carefulClose(&narrowPeakFile);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
simpleBed = optionVal("simpleBed", simpleBed);
stateProb = optionVal("stateProb", stateProb);
peakThreshold = optionDouble("peakThreshold", peakThreshold);
narrowPeak = optionVal("narrowPeak", narrowPeak);
regBeadPos(argv[1], argv[2]);
return 0;
}
