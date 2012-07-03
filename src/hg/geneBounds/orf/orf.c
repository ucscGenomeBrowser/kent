/* orf - Find orf for cDNAs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "orf - Find orf for cDNAs\n"
  "usage:\n"
  "   orf orf.stats mrna.fa output\n"
  "options:\n"
  "   -checkIn=seq.cds - File that says size and cds position of sequences\n"
  "   -checkOut=twinOrf.check - File that compares our cds position to real\n"
  "   -cdsOut=out.cds - File that just says where ORF we predict is and score,not rest\n"
  );
}

struct mrnaInfo
/* Cds position and related stuff - just for testing. */
    {
    struct mrnaInfo *next;
    char *name;		/* Allocated in hash */
    int size;		/* Size of mRNA */
    int cdsStart;	/* Start of coding. */
    int cdsEnd;		/* End of coding. */
    };

#define neverProb (1.0E-100)

double never;	/* Probability that should never happen */
double always;	/* Probability that should always happen */
double log4;	/* Log of 4 - used as background sometimes. */

typedef UBYTE State;    /* A state. We'll only have 256 at most for now. */

enum aStates
/* Internal states for HMM. */
{
    aUtr5,                      /* 5' UTR. */

    aKoz0, aKoz1, aKoz2, aKoz3, aKoz4, /* Kozak consensus */
    aStart1, aStart2, aStart3,  /* Start codon. */
    aC1, aC2, aC3,              /* Regular codon. */
    aStop1, aStop2, aStop3,     /* Stop codon. */

    aUtr3,			/* 3' UTR. */

    aStateCount,		/* Keeps track of total states */
};

char aVisStates[] =
/* User visible states of HMM. */
{
    'U',		     /* 5' UTR. */

    'k', 'k', 'k', 'k', 'k',  /* Kozak preamble. */
    'A', 'A', 'A',  /* Start codon. */
    '1', '2', '3',               /* Regular codon. */
    'Z', 'Z', 'Z',        /* Stop codon. */

    'u',                     /* 3' UTR. */

    'x',	/* bad state. */
};

struct trainingData
/* Stuff loaded from training stats file. */
    {
    struct trainingData *next;
    double utr5[4][4];		/* 5' UTR Markov model */
    double utr3[4][4];		/* 3' UTR Markov model */
    double codon[4][4][4];	/* Codon table */
    double kozak[10][4];	/* Kozak consensus */
    };

double scaleLog(double x)
/* Scale log (or not) and check for zero */
{
if (x <= 0)
    return never;
return log(x);
}

void readMatrix2(struct lineFile *lf, double m[4][4])
/* Read two dimensional matrix. */
{
char *row[5];
int i,j;
for (i=0; i<4; ++i)
    {
    lineFileRow(lf, row);
    if (row[0][0] != valToNt[i])
        errAbort("Expecting %c to start line %d of %s", valToNt[i], lf->lineIx, lf->fileName);
    for (j=0; j<4; ++j)
	{
	m[i][j] = scaleLog(atof(row[j+1]));
	// uglyf("%c%c %f\n", valToNt[i], valToNt[j], m[i][j]);
	}
    }
}


void readMatrix3(struct lineFile *lf, double m[4][4][4])
/* Read three dimensional matrix. */
{
int i,j,k;
char start[4];
start[3] = 0;
for (i=0; i<4; ++i)
    {
    start[0] = valToNt[i];
    for (j=0; j<4; ++j)
	{
	start[1] = valToNt[j];
	for (k=0; k<4; ++k)
	    {
	    char *row[2];
	    lineFileRow(lf, row);
	    start[2] = valToNt[k];
	    if (!sameString(start, row[0]))
	        errAbort("Expecting %s to start line %d of %s",
			start, lf->lineIx, lf->fileName);
	    m[i][j][k] = scaleLog(atof(row[1]));
	    }
	}
    }
}

void readKozak(struct lineFile *lf, double kozak[10][4])
/* Read in kozak consensus */
{
int i, j;
for (i=0; i<10; ++i)
    {
    char *row[4];
    lineFileRow(lf, row);
    for (j=0; j<4; ++j)
         kozak[i][j] = scaleLog(atof(row[j]));
    }
}

struct trainingData *loadTrainingData(char *fileName)
/* Load training data from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct trainingData *td;
char *row[10], *type;
boolean gotUtr3 = FALSE, gotUtr5 = FALSE, gotCodon = FALSE,
	gotKozak = FALSE;

AllocVar(td);
while (lineFileNext(lf, &line, NULL))
     {
     line = skipLeadingSpaces(line);
     if (line[0] == 0 || line[0] == '#')
         continue;
     chopLine(line, row);
     type = row[0];
     if (sameString(type, "utr3"))
	 {
	 // uglyf("utr3\n");
         readMatrix2(lf, td->utr3);
	 gotUtr3 = TRUE;
	 }
     else if (sameString(type, "utr5"))
	 {
	 // uglyf("utr5\n");
         readMatrix2(lf, td->utr5);
	 gotUtr5 = TRUE;
	 }
     else if (sameString(type, "codons"))
	 {
         readMatrix3(lf, td->codon);
	 gotCodon = TRUE;
	 }
     else if (sameString(type, "kozak"))
         {
	 readKozak(lf, td->kozak);
	 gotKozak = TRUE;
	 }
     else
         {
	 /* If don't recognize type skip to next blank line. */
	 while (lineFileNext(lf, &line, NULL))
	     {
	     line = skipLeadingSpaces(line);
	     if (line[0] == 0)
	         break;
	     }
	 }
     }
if (!gotUtr3 || !gotUtr5 || !gotCodon || !gotKozak)
    errAbort("Need utr3, utr5, kozak and codon in %s", fileName);
return td;
}

struct dynoData
/* Data structures for running dynamic program. */
    {
    struct dynoData *next;
    int stateCount;
    double *scoreColumns[2];		/* Memory for two columns of scores. */
    double *curScores, *prevScores;	/* Current and previous scores. */
    double *transProbLookup[256];	/* Transition probabilities. */
    int scoreFlopper;			/* Flops between 0 and 1. */
    };

static void dynoFlopScores(struct dynoData *dd)
/* Advance to next column. */
{
dd->scoreFlopper = 1-dd->scoreFlopper;
dd->curScores = dd->scoreColumns[dd->scoreFlopper];
dd->prevScores = dd->scoreColumns[1-dd->scoreFlopper];
}

struct dynoData *newDynoData(int stateCount)
/* Allocate and initialize score columns. */
{
struct dynoData *dyno;
int i, j;

AllocVar(dyno);
dyno->stateCount = stateCount;
AllocArray(dyno->scoreColumns[0], stateCount);
AllocArray(dyno->scoreColumns[1], stateCount);
dynoFlopScores(dyno);

/* In this loop allocate transition table and set all values to
 * something improbable, but not so improbable that the log value
 * will overflow. */
for (i=0; i<stateCount; ++i)
    {
    AllocArray(dyno->transProbLookup[i], stateCount);
    for (j=0; j<stateCount; ++j)
        dyno->transProbLookup[i][j] = never;
    }

return dyno;
}

void makeTransitionProbs(double **transProbLookup)
/* Allocate transition probabilities and initialize them. */
{
/* Make certain transitions reasonably likely. */
transProbLookup[aUtr5][aUtr5] = scaleLog(0.990);
transProbLookup[aUtr5][aKoz0] = scaleLog(0.010);
transProbLookup[aKoz0][aKoz1] = always;
transProbLookup[aKoz1][aKoz2] = always;
transProbLookup[aKoz2][aKoz3] = always;
transProbLookup[aKoz3][aKoz4] = always;
transProbLookup[aKoz4][aStart1] = always;
transProbLookup[aStart1][aStart2] = always;
transProbLookup[aStart2][aStart3] = always;
transProbLookup[aStart3][aC1] = always;
transProbLookup[aC1][aC2] = always;
transProbLookup[aC2][aC3] = always;
transProbLookup[aC3][aC1] = scaleLog(0.9970);
transProbLookup[aC3][aStop1] = scaleLog(0.003);
transProbLookup[aStop1][aStop2] = always;
transProbLookup[aStop2][aStop3] = always;
transProbLookup[aStop3][aUtr3] =   always;
transProbLookup[aUtr3][aUtr3] = always;
}

#define startState(curState) \
    { \
    int destState = curState; \
    double newScore = -1000000000.0; \
    State parent = aStateCount; \
    double oneScore;

#define endState(curState) \
    dyno->curScores[destState] = newScore; \
    allStates[destState][dnaIx] = parent; \
    }

    // int ufoo = uglyf(" start %d(%c)\n", curState, aVisStates[curState]);
    // uglyf(" end %d(%c) from %d(%c) score %f\n", curState, aVisStates[curState],
    //       parent, aVisStates[parent], newScore);
    // uglyf("   source %d(%c) transProb %f, emitScore %f, prevScore %f, t+e %f\n", sourceState,
    //       aVisStates[sourceState], dyno->transProbLookup[sourceState][destState], emitScore,
    //       dyno->prevScores[sourceState],
    //       dyno->transProbLookup[sourceState][destState] + emitScore);

#define source(sourceState, emitScore) \
    if ((oneScore = dyno->transProbLookup[sourceState][destState] + emitScore + dyno->prevScores[sourceState]) > newScore) \
        { \
        newScore = oneScore; \
        parent = sourceState; \
        }


double traceBack(double *scores, State **allStates, struct dnaSeq *seq, FILE *f,
	struct hash *checkInHash, FILE *checkOut, FILE  *cdsOut)
/* Trace back, adding symbol track.  Then print out result.  Return score. */
{
int i;
double maxScore = -0x3fffffff;
int maxState = 0;
int lineSize;
int maxLineSize = 50;
State *states;
int symCount = seq->size;
State *tStates = needMem(symCount * sizeof(tStates[0]));

/* Find end state. */
for (i=0; i<aStateCount; ++i)
    {
    if (scores[i] > maxScore)
        {
        maxScore = scores[i];
        maxState = i;
        }
    }
fprintf(f, ">%s %f\n", seq->name, maxScore);

for (i = symCount-1; i >= 0; i -= 1)
    {
    tStates[i] = aVisStates[maxState];
    states = allStates[maxState];
    maxState = states[i];
    }

for (i=0; i<symCount; i += lineSize)
    {
    lineSize = symCount - i;
    if (lineSize > maxLineSize)
        lineSize = maxLineSize;
    mustWrite(f, seq->dna+i, lineSize);
    fprintf(f, " %d\n", i+lineSize);
    mustWrite(f, tStates+i, lineSize);
    fputc('\n', f);
    fputc('\n', f);
    }

static char isCdsChar[256];
isCdsChar['A'] = TRUE;
isCdsChar['Z'] = TRUE;
isCdsChar['1'] = TRUE;
isCdsChar['2'] = TRUE;
isCdsChar['3'] = TRUE;
int cdsStart = symCount, cdsEnd = 0;
for (i=0; i<symCount; ++i)
    {
    if (isCdsChar[tStates[i]])
	{
	if (i < cdsStart)
	     cdsStart = i;
	if (i >= cdsEnd)
	     cdsEnd = i+1;
	}
    }
if (checkInHash != NULL && checkOut != NULL)
    {
    struct mrnaInfo *mi = hashMustFindVal(checkInHash, seq->name);
    fprintf(checkOut, "%s\t%d\t%d\t%d\t%d\t%d\n",
        mi->name, mi->size, mi->cdsStart, mi->cdsEnd,
	cdsStart, cdsEnd);
    }
if (cdsOut != NULL)
    fprintf(cdsOut, "%s\t%d\t%d\t%d\t%f\n", seq->name, seq->size, cdsStart, cdsEnd, maxScore);

freeMem(tStates);
return maxScore;
}

double kozakProb(double kozak[10][4], char *dna)
/* Return probability of kozak consensus. */
{
double odds = 0;
int i;
for (i=0; i<10; ++i)
    odds += kozak[i][ntVal[(int)dna[i]]];
odds += log4;	/* Correct for double use of last two nucleotides. */
odds += log4;
// uglyf("  kozakProb %c%c%c%c%c^%c%c%c%c%c = %f\n", dna[0], dna[1], dna[2], dna[3], dna[4], dna[5], dna[6], dna[7], dna[8], dna[9], odds);
return odds;
}


double fullDynamo(struct trainingData *td, struct dynoData *dyno,
	struct dnaSeq *seq,  FILE *f, struct hash *checkInHash,
	FILE *checkOut, FILE *cdsOut)
/* Run dynamic programming algorithm on HMM. Return score. */
{
DNA *dna = seq->dna;
DNA *pos;
int base, lastBase = 0, last2base = 0;
int dnaSize = seq->size;
State **allStates;
int stateCount = dyno->stateCount;
int stateByteSize = dnaSize * sizeof(State);
int i;
int dnaIx;
int scanSize = dnaSize;
double score;
double kozakAdjustment = 0.0;

/* Allocate state tables. */
allStates = needMem(stateCount * sizeof(allStates[0]));
for (i=0; i<stateCount; ++i)
    {
    allStates[i] = needLargeMem(stateByteSize);
    memset(allStates[i], 0, stateByteSize);
    }

/* Initialize score columns, and set up transitions from start state. */
for (i=0; i<stateCount; ++i)
    dyno->prevScores[i] = never;
dyno->prevScores[aUtr5] = scaleLog(0.60);
dyno->prevScores[aC1] = scaleLog(0.10);
dyno->prevScores[aC2] = scaleLog(0.10);
dyno->prevScores[aC3] = scaleLog(0.10);
dyno->prevScores[aUtr3] = scaleLog(0.20);

for (dnaIx=0; dnaIx<scanSize; dnaIx += 1)
    {
    pos = dna+dnaIx;
    // uglyf("%d %c\n", dnaIx, pos[0]);
    base = ntVal[(int)pos[0]];

    /* Utr5 state. */
    startState(aUtr5)
        double b = td->utr5[lastBase][base];
        source(aUtr5, b);
    endState(aUtr5)

    /* Near start codon states. */
    startState(aKoz0)
	source(aUtr5, always);
    endState(aKoz0)

    startState(aKoz1)
	source(aKoz0, always);
    endState(aKoz1)

    startState(aKoz2)
	source(aKoz1, always);
    endState(aKoz2)

    startState(aKoz3)
	source(aKoz2, always);
    endState(aKoz3)

    startState(aKoz4)
	source(aKoz3, always);
    endState(aKoz4)

    startState(aStart1)
	source(aKoz4, always);
    endState(aStart1)

    startState(aStart2)
	source(aStart1, always);
    endState(aStart2)

    startState(aStart3)
	double b;
	if (dnaIx < 7 || scanSize - dnaIx < 3)
	    b = never;
	else
	    {
	    double kOdds = kozakProb(td->kozak, pos-7);
	    b = kOdds + kozakAdjustment;
	    if (kOdds > 8.0)
	        kozakAdjustment -= 4.0;	/* Prefer first reasonable one. */
	    }
	source(aStart2, b);
    endState(aStart3)

    /* Coding main states */
    startState(aC1)
        source(aC3, always);
        source(aStart3, always);
    endState(aC1)

    startState(aC2)
        source(aC1, always);
    endState(aC2)

    startState(aC3)
	if (dnaIx >= 2)
	    {
	    double b = td->codon[last2base][lastBase][base];
	    // uglyf(" C3: %c%c%c = %f\n", valToNt[last2base], valToNt[lastBase], valToNt[base], b);
	    source(aC2, b);
	    }
    endState(aC3)


    /* Stop codon */
    startState(aStop1)
        source(aC3, always);
    endState(aStop1)

    startState(aStop2)
        source(aStop1, always);
    endState(aStop2)

    startState(aStop3)
        double b = never;
	if (dnaIx >= 2)
	    {
	    if (last2base == T_BASE_VAL)
		{
		if (lastBase == A_BASE_VAL)
		    {
		    if (base == G_BASE_VAL || base == A_BASE_VAL)
			b = always;
		    }
		else if (lastBase == G_BASE_VAL)
		    {
		    if (base == A_BASE_VAL)
			b = always;
		    }
		}
	    }
        source(aStop2, b);
    endState(aStop3)


    /* UTR states. */
    startState(aUtr3)
        double b = td->utr3[lastBase][base];
        source(aUtr3, b);
	source(aStop3, b);
    endState(aUtr3)

    dynoFlopScores(dyno);
    last2base = lastBase;
    lastBase = base;
    }


/* Find best scoring final cell and trace backwards to
 * reconstruct full path. */
score = traceBack(dyno->prevScores, allStates, seq, f, checkInHash, checkOut, cdsOut);

/* Clean up and return. */
for (i=0; i<stateCount; ++i)
    freeMem(allStates[i]);
freeMem(allStates);
return score;
}

void oneOrf(struct trainingData *td, struct dynoData *dyno, struct dnaSeq *seq, FILE *f,
	struct hash *checkInHash, FILE *checkOut, FILE *cdsOut)
/* Try and find one orf in axt. */
{
fullDynamo(td, dyno, seq, f, checkInHash, checkOut, cdsOut);
}

struct hash *readCheckIn(char *fileName)
/* Read in file into a hash of mrnaInfos. */
{
if (fileName == NULL)
    return NULL;
else
    {
    struct hash *hash = newHash(16);
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *row[4];
    while (lineFileRow(lf, row))
	{
	struct mrnaInfo *m;
	AllocVar(m);
	hashAddSaveName(hash, row[0], m, &m->name);
	m->size = lineFileNeedNum(lf, row, 1);
	m->cdsStart = lineFileNeedNum(lf, row, 2);
	m->cdsEnd = lineFileNeedNum(lf, row, 3);
	}
    return hash;
    }
}

void orf(char *statsFile, char *faFile, char *outFile, char *checkInFile, char *checkOutFile,
	char *cdsOutFile)
/* orf - Find orf for cDNAs. */
{
struct trainingData *td = loadTrainingData(statsFile);
struct dynoData *dyno = newDynoData(aStateCount);
struct lineFile *lf = lineFileOpen(faFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dnaSeq seq;
struct hash *checkInHash = readCheckIn(checkInFile);
FILE *checkOut = NULL;
FILE *cdsOut = NULL;

if (checkOutFile == NULL && checkInFile != NULL)
    errAbort("CheckOut without checkIn");
if (checkOutFile != NULL)
     checkOut = mustOpen(checkOutFile, "w");
if (cdsOutFile != NULL)
     cdsOut = mustOpen(cdsOutFile, "w");

makeTransitionProbs(dyno->transProbLookup);
ZeroVar(&seq);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    oneOrf(td, dyno, &seq, f, checkInHash, checkOut, cdsOut);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
never = scaleLog(neverProb);
always = scaleLog(1.0);
log4 = scaleLog(4.0);
if (argc != 4)
    usage();
orf(argv[1], argv[2], argv[3], optionVal("checkIn", NULL), optionVal("checkOut", NULL),
	optionVal("cdsOut", NULL));
return 0;
}
