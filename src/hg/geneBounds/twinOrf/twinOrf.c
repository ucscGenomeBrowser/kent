/* twinOrf - Predict open reading frame in cDNA given a cross species alignment. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrf - Predict open reading frame in cDNA given a cross species alignment\n"
  "usage:\n"
  "   twinOrf twinOrf.stats in.axt out.orf\n"
  "options:\n"
  "   -checkIn=seq.cds - File that says size and cds position of sequences\n"
  "   -checkOut=twinOrf.check - File that compares our cds position to real\n"
  "   -m1  - Use markov 1 model for UTRs  (currently has weird results\n"
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

#define scaledLog log
#define neverProb (1.0E-100)

double never;	/* Probability that should never happen */
double always;	/* Probability that should always happen */
boolean useM1 = FALSE; /* Use M1 models for UTRs */

enum aStates 
/* Internal states for HMM. */
{
    aUtr5,                      /* 5' UTR. */
    aUtr3,			/* 3' UTR. */
    
    aKoz0, aKoz1, aKoz2, aKoz3, aKoz4,  /* Kozak preamble. */
    aStart1, aStart2, aStart3,  /* Start codon. */
    aC1, aC2, aC3,              /* Regular codon. */
    aStop1, aStop2, aStop3,     /* Stop codon. */

    aInsC1, aInsC2, aInsC3,	/* Single inserts in coding. */
    aI1C1, aI2C1, aI3C1,	/* Multiple of 3 inserts in coding. */
    aI1C2, aI2C2, aI3C2,	/* Multiple of 3 inserts in coding. */
    aI1C3, aI2C3, aI3C3,	/* Multiple of 3 inserts in coding. */


    aStateCount,		/* Keeps track of total states */
};

char visStates[] =
/* User visible states of HMM. */
{
    'U',		     /* 5' UTR. */
    'u',                     /* 3' UTR. */
    
    'k', 'k', 'k', 'k', 'k',  /* Kozak */
    'A', 'A', 'A',  /* Start codon. */
    '1', '2', '3',               /* Regular codon. */
    'Z', 'Z', 'Z',        /* Stop codon. */

    '^', '^', '^',        /* Inserts in coding. */
    '-', '-', '-',        /* Multiple of 3 inserts in coding. */
    '-', '-', '-',        /* Multiple of 3 inserts in coding. */
    '-', '-', '-',        /* Multiple of 3 inserts in coding. */

    'x',	/* bad state. */
};

typedef UBYTE State;    /* A state. We'll only have 256 at most for now. */

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
transProbLookup[aUtr5][aUtr5] = scaledLog(0.990);
transProbLookup[aUtr5][aKoz0] = scaledLog(0.010);
transProbLookup[aKoz0][aKoz1] = always;
transProbLookup[aKoz1][aKoz2] = always;
transProbLookup[aKoz2][aKoz3] = always;
transProbLookup[aKoz3][aKoz4] = always;
transProbLookup[aKoz4][aStart1] = always;
transProbLookup[aStart1][aStart2] = always;
transProbLookup[aStart2][aStart3] = always;
transProbLookup[aStart3][aC1] = always;
transProbLookup[aC1][aC2] = scaledLog(0.9999);
transProbLookup[aC2][aC3] = scaledLog(0.9999);
transProbLookup[aC3][aC1] = scaledLog(0.9969);
transProbLookup[aC3][aStop1] = scaledLog(0.003);
transProbLookup[aStop1][aStop2] = always;
transProbLookup[aStop2][aStop3] = always;
transProbLookup[aStop3][aUtr3] =   always;
transProbLookup[aUtr3][aUtr3] = always;
transProbLookup[aC1][aInsC1] = scaledLog(0.0001);
transProbLookup[aC2][aInsC2] = scaledLog(0.0001);
transProbLookup[aC3][aInsC3] = scaledLog(0.0001);
transProbLookup[aInsC1][aC2] = scaledLog(0.99);
transProbLookup[aInsC1][aInsC1] = scaledLog(0.01);
transProbLookup[aInsC2][aC3] = scaledLog(0.99);
transProbLookup[aInsC2][aInsC2] = scaledLog(0.01);
transProbLookup[aInsC3][aC1] = scaledLog(0.99);
transProbLookup[aInsC3][aInsC3] = scaledLog(0.01);
transProbLookup[aC1][aI1C1] = scaledLog(0.001);
transProbLookup[aI1C1][aI2C1] = always;
transProbLookup[aI2C1][aI3C1] = always;
transProbLookup[aI3C1][aI1C1] = scaledLog(0.2);
transProbLookup[aI3C1][aC2] = scaledLog(0.8);
transProbLookup[aC2][aI1C2] = scaledLog(0.001);
transProbLookup[aI1C2][aI2C2] = always;
transProbLookup[aI2C2][aI3C2] = always;
transProbLookup[aI3C2][aI1C2] = scaledLog(0.2);
transProbLookup[aI3C2][aC3] = scaledLog(0.8);
transProbLookup[aC3][aI1C3] = scaledLog(0.001);
transProbLookup[aI1C3][aI2C3] = always;
transProbLookup[aI2C3][aI3C3] = always;
transProbLookup[aI3C3][aI1C3] = scaledLog(0.2);
transProbLookup[aI3C3][aC1] = scaledLog(0.8);
}

int countReal(char *s, int size)
/* Count chars not '-' or '.' */
{
int i, count = 0;
char c;
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c != '-' && c != '.')
        ++count;
    }
return count;
}

double upcTraceback(char *name, double *scores, State **allStates, struct axt *axt, 
	struct hash *checkInHash, FILE *checkOut, FILE *f)
/* Trace back, adding symbol track.  Then print out result.  Return score. */
{
int i;
double maxScore = -0x3fffffff;
int maxState = 0;
int lineSize;
int maxLineSize = 50;
State *states;
State *tStates = needMem(axt->symCount * sizeof(tStates[0]));
int symCount = axt->symCount;
int qNum = 0, tNum = 0;

/* Find end state. */
for (i=0; i<aStateCount; ++i)
    {
    if (scores[i] > maxScore)
        {
        maxScore = scores[i];
        maxState = i;
        }
    }
fprintf(f, ">%s %f\n", name, maxScore);

for (i = symCount-1; i >= 0; i -= 1)
    {
    tStates[i] = visStates[maxState];
    states = allStates[maxState];
    maxState = states[i];
    }

for (i=0; i<symCount; i += lineSize)
    {
    lineSize = symCount - i;
    if (lineSize > maxLineSize)
        lineSize = maxLineSize;
    qNum += countReal(axt->qSym+i, lineSize);
    tNum += countReal(axt->tSym+i, lineSize);
    mustWrite(f, axt->tSym+i, lineSize);
    fprintf(f, " %d\n", tNum);
    mustWrite(f, axt->qSym+i, lineSize);
    fprintf(f, " %d\n", qNum);
    mustWrite(f, tStates+i, lineSize);
    fputc('\n', f);
    fputc('\n', f);
    }

if (checkInHash != NULL && checkOut != NULL)
    {
    struct mrnaInfo *mi = hashMustFindVal(checkInHash, axt->tName);
    int i, cdsStart = symCount, cdsEnd = 0;
    static char isCdsChar[256];
    isCdsChar['A'] = TRUE;
    isCdsChar['Z'] = TRUE;
    isCdsChar['1'] = TRUE;
    isCdsChar['2'] = TRUE;
    isCdsChar['3'] = TRUE;
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
    fprintf(checkOut, "%s\t%d\t%d\t%d\t%d\t%d\n", 
    	mi->name, mi->size, mi->cdsStart, mi->cdsEnd, 
	countReal(axt->tSym, cdsStart),
	countReal(axt->tSym, cdsEnd));
    }
freeMem(tStates);
return maxScore;
}


char ixToSym[] = {'T', 'C', 'A', 'G', 'N', '-', '.'};
char lixToSym[] = {'t', 'c', 'a', 'g', 'n', '-', '.'};
int symToIx[256];

void initSymToIx()
/* Initialize lookup table. */
{
int i;
for (i=0; i<ArraySize(symToIx); ++i)
    symToIx[i] = -1;
for (i=0; i<ArraySize(ixToSym); ++i)
    {
    symToIx[(int)ixToSym[i]] = i;
    symToIx[(int)lixToSym[i]] = i;
    }
}

struct baseOdds
/* Log odds matrix */
    {
    double odds[7][7];
    int observations;		/* Total # of observations based on. */
    };

struct markov1
/* A matrix for markov1 odds. */
    {
    double odds[7*7][7*7];
    int observations;
    };

struct codonOdds
/* A matrix for codon odds */
    {
    double odds[7*7*7][7*7*7];
    int observations;		/* Total # of observations based on. */
    };

struct baseOdds *readBaseOdds(struct lineFile *lf, char **initRow)
/* Read a base odds table. */
{
struct baseOdds *o;
int i, j;
double p;
char *row[7+1], *name;
AllocVar(o);
o->observations = atoi(initRow[1]);
for (i=0; i<7; ++i)
    {
    lineFileRow(lf, row);
    name = row[0];
    if (name[1] != 0 || name[0] != ixToSym[i])
        errAbort("Expecting line starting with %c line %d of %s", 
		ixToSym[i], lf->lineIx, lf->fileName);
    for (j=0; j<7; ++j)
	{
	p = atof(row[j+1]);
	if (p <= 0) p = neverProb;
	o->odds[i][j] = scaledLog(p);
	}
    }
return o;
}

void dumpBaseOdds(struct baseOdds *o)
{
int i, j;
for (i=0; i<7; ++i)
   {
   printf("%c", ixToSym[i]);
   for (j=0; j<7; ++j)
      printf(" %f", o->odds[i][j]);
   printf("\n");
   }
}

struct codonOdds *readCodonOdds(struct lineFile *lf, char **initRow)
/* Read a codon odds table.  Convert to logs. */
{
struct codonOdds *o;
int i,j;
double p;
char *row[7*7*7+1], *name;
AllocVar(o);
o->observations = atoi(initRow[1]);
for (i=0; i<7*7*7; ++i)
    {
    /* Decompose i into codons. */
    int c[3];
    int k, ic=i;
    for (k=2; k>=0; --k)
        {
	c[k] = ic%7;
	ic /= 7;
	}
    /* Read line and make sure first word matches. */
    lineFileRow(lf, row);
    name = row[0];
    if (name[0] != ixToSym[c[0]] || name[1] != ixToSym[c[1]] || name[2] != ixToSym[c[2]])
        {
	errAbort("Expecting %c%c%c line %d of %s\n",
		ixToSym[c[0]], ixToSym[c[1]], ixToSym[c[2]], lf->lineIx, lf->fileName);
	}
    for (j=0; j<7*7*7; ++j)
	{
	p = atof(row[j+1]);
	if (p <= 0) p = neverProb;
	o->odds[i][j] = scaledLog(p);
	}
    }
return o;
}

struct markov1 *readM1Odds(struct lineFile *lf, char **initRow)
/* Read a 1st order Markov odds table. Don't convert to logs. */
{
struct  markov1 *o;
int i,j;
double p;
char *row[7*7+1], *name;
AllocVar(o);
o->observations = atoi(initRow[1]);
for (i=0; i<7*7; ++i)
    {
    /* Decompose i into pairs. */
    int c[2];
    int k, ic=i;
    for (k=1; k>=0; --k)
        {
	c[k] = ic%7;
	ic /= 7;
	}
    /* Read line and make sure first word matches. */
    lineFileRow(lf, row);
    name = row[0];
    if (name[0] != ixToSym[c[0]] || name[1] != ixToSym[c[1]])
        {
	errAbort("Expecting %c%c line %d of %s\n",
		ixToSym[c[0]], ixToSym[c[1]], lf->lineIx, lf->fileName);
	}
    for (j=0; j<7*7; ++j)
	{
	p = atof(row[j+1]);
	if (p <= 0) p = neverProb;
	o->odds[i][j] = p;
	}
    }
return o;
}

void m1Normalize(struct markov1 *o)
/* Normalize rows of markov1 matrix  and convert to logs. */
{
int i,j;

for (i=0; i<7*7; ++i)
   {
   double rowTotal = 0;
   for (j=0; j<7*7; ++j)
       rowTotal += o->odds[i][j];
   if (rowTotal > 0)
       {
       for (j=0; j<7*7; ++j)
	   o->odds[i][j] = scaledLog(o->odds[i][j]/rowTotal);
       }
   }
}

void downgradeM1(struct markov1 *o, double amount)
/* Reduce odds of m */
{
int i,j;
for (i=0; i<7*7; ++i)
    for (j=0; j<7*7; ++j)
        o->odds[i][j] -= amount;
}


struct markov1 *m1FromCountsInFile(struct lineFile *lf, char **initRow)
/* Read in counts from file and convert to markov model. */
{
struct markov1 *o = readM1Odds(lf, initRow);
m1Normalize(o);
return o;
}

int ixForCodon(char c[3])
/* Return index form of codon. */
{
int i;
int ix = 0;
for (i=0; i<3; ++i)
   {
   ix *= 7;
   ix += symToIx[(int)c[i]];
   }
return ix;
}

void killStopCodons(struct codonOdds *o)
/* Set any target sequence involving stop codons to very low probability */
{
static char *stopCodons[] = {"taa", "tag", "tga"};
int i, j, ix;
for (i=0; i<3; ++i)
    {
    ix = ixForCodon(stopCodons[i]);
    for (j=0; j<7*7*7; ++j)
        o->odds[ix][j] = -2000;
    }
}

void killInsertsInCodons(struct codonOdds *o)
/* Get rid of inserts in the target.  Scale down
 * inserts in the query except for '---' */
{
int i1,i2,i3;
int i,j;
for (i1=0; i1<7; ++i1)
    {
    for (i2=0; i2<7; ++i2)
        {
	for (i3=0; i3<7; ++i3)
	    {
	    if (i1 == '-' || i2 == '-' || i3 == '-')
	         {
		 i = i1*7*7 + i2*7 + i3;
		 for (j=0; j<7*7*7; ++j)
		     o->odds[i][j] = -2000;
		 }
	    }
	}
    }
for (i1=0; i1<7; ++i1)
    {
    for (i2=0; i2<7; ++i2)
        {
	for (i3=0; i3<7; ++i3)
	    {
	    int dashCount = 0;
	    if (i1 == '-') ++dashCount;
	    if (i2 == '-') ++dashCount;
	    if (i3 == '-') ++dashCount;
	    if (dashCount > 0)
	         {
		 int penalty = (dashCount == 3 ? 4 : 12);
		 i = i1*7*7 + i2*7 + i3;
		 for (j=0; j<7*7*7; ++j)
		     o->odds[j][i] -= penalty;
		 }
	    }
	}
    }
}

struct trainingData
/* Stuff loaded from training stats file. */
    {
    struct trainingData *next;
    struct baseOdds *utr5;	/* 5' UTR */
    struct baseOdds *utr3;	/* 3' UTR */
    struct baseOdds *kozak[10]; /* Kozak (start codon) consensus */
    struct markov1  *m1Utr5;	/* Markov 1st order of 5' UTR */
    struct markov1  *m1Utr3;	/* Markov 1st order of 3' UTR */
    struct codonOdds *codon;	/* Codon/codon */
    };

struct trainingData *loadTrainingData(char *fileName)
/* Load training data from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct trainingData *td;
char *row[10], *type;
int i;

AllocVar(td);
while (lineFileNext(lf, &line, NULL))
     {
     line = skipLeadingSpaces(line);
     if (line[0] == 0 || line[0] == '#')
         continue;
     chopLine(line, row);
     type = row[0];
     if (sameString(type, "utr3"))
         td->utr3 = readBaseOdds(lf, row);
     else if (sameString(type, "utr5"))
         td->utr5 = readBaseOdds(lf, row);
     else if (startsWith("kozak[", type))
         {
	 int ix = atoi(type + strlen("kozak["));
	 ix += 5;
	 if (ix < 0 || ix >= 10)
	     errAbort("kozak out of range line %d of %s", lf->lineIx, lf->fileName);
	 td->kozak[ix] = readBaseOdds(lf, row);
	 }
     else if (sameString(type, "c2_utr5"))
	 td->m1Utr5 = m1FromCountsInFile(lf, row);
     else if (sameString(type, "c2_utr3"))
	 td->m1Utr3 = m1FromCountsInFile(lf, row);
     else if (sameString(type, "codon"))
         td->codon = readCodonOdds(lf, row);
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
lineFileClose(&lf);
if (td->utr3 == NULL)
    errAbort("Missing 'utr3' record from %s\n", fileName);
if (td->utr5 == NULL)
    errAbort("Missing 'utr5' record from %s\n", fileName);
if (td->codon == NULL)
    errAbort("Missing 'codon' record from %s\n", fileName);
if (td->m1Utr5 == NULL)
    errAbort("Missing 'c1_utr5 record from %s\n", fileName);
if (td->m1Utr3 == NULL)
    errAbort("Missing 'c1_utr3 record from %s\n", fileName);
for (i=0; i<10; ++i)
    {
    if (td->kozak[i] == NULL)
	errAbort("Missing 'kozak[%d]' record from %s\n", i, fileName);
    }
downgradeM1(td->m1Utr5, 0.1);
downgradeM1(td->m1Utr3, 0.1);
killStopCodons(td->codon);
killInsertsInCodons(td->codon);
return td;
}

double prob1(struct baseOdds *o, char q, char t)
/* Find probability of simple q/t pair. */
{
return o->odds[symToIx[(int)t]][symToIx[(int)q]];
}

double probM1(struct markov1 *o, char *q, char *t)
/* Find probability of double */
{
int qIx = symToIx[(int)t[0]]*7 + symToIx[(int)q[0]];
int tIx = symToIx[(int)t[1]]*7 + symToIx[(int)q[1]];
double x;
x = o->odds[tIx][qIx];
// uglyf("  probM1(%c%c %c%c) = %f\n", t[0], t[1], q[0], q[1], x);
return x;
}


double prob3(struct codonOdds *o, char *q, char *t)
/* Find probability of triple. */
{
int qIx = 0, tIx = 0, i;
double x;
for (i=0; i<3; ++i)
    {
    qIx *= 7;
    qIx += symToIx[(int)q[i]];
    tIx *= 7;
    tIx += symToIx[(int)t[i]];
    }
x =  o->odds[tIx][qIx];
// uglyf("  prob3(%s %s) = %f\n", t, q, x);
return x;
}

#define startState(curState) \
    { \
    int destState = curState; \
    double newScore = -1000000000.0; \
    State parent = aStateCount; \
    double oneScore; 

#define endState(curState) \
    dyno->curScores[destState] = newScore; \
    allStates[destState][symIx] = parent; \
    }

    // uglyf(" %d(%c) from %d(%c) score %f\n", curState, visStates[curState], parent, visStates[parent], newScore); 

#define source(sourceState, emitScore) \
    if ((oneScore = dyno->transProbLookup[sourceState][destState] + emitScore + dyno->prevScores[sourceState]) > newScore) \
        { \
        newScore = oneScore; \
        parent = sourceState; \
        }


boolean getPrevNonInsert(char *qSym, char *tSym, 
	int symIx, char *qRes, char *tRes, int resSize)
/* Get last bases without inserts in t into qRes/tRes. */
{
int baseIx = resSize - 1;
int i;
for (i=symIx; i >= 0; --i)
    {
    if (tSym[i] != '-')
        {
	tRes[baseIx] = tSym[i];
	qRes[baseIx] = qSym[i];
	--baseIx;
	if (baseIx < 0)
	    break;
	}
    }
return (baseIx < 0);
}

double lastCodonProb(struct codonOdds *o, char *qSym, char *tSym, int symIx)
/* Return probability of last codon. */
{
static char qCodon[4], tCodon[4];
if (!getPrevNonInsert(qSym, tSym, symIx, qCodon, tCodon, 3))
    return never;
// uglyf("%c%c%c %c%c%c  vs %c%c%c %c%c%c\n", tSym[symIx-2], tSym[symIx-1], tSym[symIx], qSym[symIx-2], qSym[symIx-1], qSym[symIx], tCodon[0], tCodon[1], tCodon[2], qCodon[0], qCodon[1], qCodon[2]);
return prob3(o, qCodon, tCodon);
}

double kozakProb(struct baseOdds *kozak[8], char *qSym, char *tSym, int symIx)
/* Return probability of kozak consensus. */
{
char qs[8], ts[8];
double odds = 0;
int i;
if (!getPrevNonInsert(qSym, tSym, symIx, qs, ts, 8))
    return never;
for (i=0; i<8; ++i)
    odds += prob1(kozak[i], qs[i], ts[i]);
return odds;
}

double fullDynamo(struct trainingData *td, struct dynoData *dyno, struct axt *axt, 
	struct hash *checkInHash, FILE *checkOut, FILE *f)
/* Run dynamic programming algorithm on HMM. Return score. */
{
char *qSym = axt->qSym;
char *tSym = axt->tSym;
int symCount = axt->symCount;
State **allStates;
char *q, *t, tc, qc;
int stateCount = dyno->stateCount;
int stateByteSize = symCount * sizeof(State);
int i;
int symIx;
int scanSize = symCount;
double score;
double tNotIns, tIsIns;

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
dyno->prevScores[aUtr5] = scaledLog(0.60);
dyno->prevScores[aC1] = scaledLog(0.10);
dyno->prevScores[aC2] = scaledLog(0.10);
dyno->prevScores[aC3] = scaledLog(0.10);
dyno->prevScores[aUtr3] = scaledLog(0.20);

for (symIx=0; symIx<scanSize; symIx += 1)
    {
    q = qSym + symIx;
    t = tSym + symIx;
    qc = *q;
    tc = *t;
    // uglyf("%d %c %c\n", symIx, qc, tc);
    if (tc == '-')
        {
	tNotIns = never;
	tIsIns = always;
	}
    else
        {
	tNotIns = always;
	tIsIns = never;
	}

    /* Utr5 state. */
    startState(aUtr5)
        double b;
	if (symIx == 0 || !useM1)
	    b = prob1(td->utr5, qc, tc);
	else
	    b = probM1(td->m1Utr5, q-1, t-1);
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
        double b = kozakProb(td->kozak, qSym, tSym, symIx);
        source(aStart2, b);
    endState(aStart3)

    /* Coding main states */
    startState(aC1)
        source(aC3, tNotIns);	
        source(aStart3, always);
	source(aInsC3, tNotIns);
	source(aI3C3, tNotIns);
    endState(aC1)

    startState(aC2)
        source(aC1, tNotIns);
	source(aInsC1, tNotIns);
	source(aI3C1, tNotIns);
    endState(aC2)

    startState(aC3)
	double b = lastCodonProb(td->codon, qSym, tSym, symIx);
	source(aC2, b);
	source(aInsC2, tNotIns);
	source(aI3C2, tNotIns);
    endState(aC3)

    /* Coding short inserts. */
    startState(aInsC1)
        source(aC1, tIsIns);
        source(aInsC1, tIsIns);
    endState(aInsC1)

    startState(aInsC2)
        source(aC2, tIsIns);
        source(aInsC2, tIsIns);
    endState(aInsC2)

    startState(aInsC3)
        source(aC3, tIsIns);
        source(aInsC3, tIsIns);
    endState(aInsC3)

    /* Coding multiple of three inserts. */
    startState(aI1C1)
        source(aC1, tIsIns);
	source(aI3C1, tIsIns);
    endState(aI1C1);

    startState(aI2C1)
        source(aI1C1, tIsIns);
    endState(aI2C1)

    startState(aI3C1)
        source(aI2C1, tIsIns);
    endState(aI3C1)

    startState(aI1C2)
        source(aC2, tIsIns);
	source(aI3C2, tIsIns);
    endState(aI1C2);

    startState(aI2C2)
        source(aI1C2, tIsIns);
    endState(aI2C2)

    startState(aI3C2)
        source(aI2C2, tIsIns);
    endState(aI3C2)

    startState(aI1C3)
        source(aC3, tIsIns);
	source(aI3C3, tIsIns);
    endState(aI1C3);

    startState(aI2C3)
        source(aI1C3, tIsIns);
    endState(aI2C3)

    startState(aI3C3)
        source(aI2C3, tIsIns);
    endState(aI3C3)

    /* Stop codon */
    startState(aStop1)
        source(aC3, always);
    endState(aStop1)

    startState(aStop2)
        source(aStop1, always);
    endState(aStop2)

    startState(aStop3)
        double b = never;
	if (symIx >= 2)
	    {
	    if (t[-2] == 't')
		{
		if (t[-1] == 'a')
		    {
		    if (t[0] == 'g' || t[0] == 'a')
			b = always;
		    }
		else if (t[-1] == 'g')
		    {
		    if (t[0] == 'a')
			b = always;
		    }
		}
	    }
        source(aStop2, b);
    endState(aStop3)


    /* UTR states. */
    startState(aUtr3)
        double b;
	if (symIx == 0 || !useM1)
	    b = prob1(td->utr3, qc, tc);
	else
	    b = probM1(td->m1Utr3, q-1, t-1);
        source(aUtr3, b);
	source(aStop3, b);
    endState(aUtr3)
  
    dynoFlopScores(dyno);
    }


/* Find best scoring final cell and trace backwards to
 * reconstruct full path. */
score = upcTraceback(axt->tName, dyno->prevScores, allStates, axt, checkInHash, checkOut, f);

/* Clean up and return. */
for (i=0; i<stateCount; ++i)
    freeMem(allStates[i]);
freeMem(allStates);
return score;
}


void oneOrf(struct trainingData *td, struct dynoData *dyno, struct axt *axt, 
	struct hash *checkInHash, FILE *checkOut, FILE *f)
/* Try and find one orf in axt. */
{
fullDynamo(td, dyno, axt, checkInHash, checkOut, f);
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

void twinOrf(char *statsFile, char *axtFile, char *outFile, 
	char *checkInFile, char *checkOutFile)
/* twinOrf - Predict open reading frame in cDNA given a cross species alignment. */
{
struct trainingData *td = loadTrainingData(statsFile);
struct dynoData *dyno = newDynoData(aStateCount);
struct lineFile *lf = lineFileOpen(axtFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct hash *checkInHash = readCheckIn(checkInFile);
FILE *checkOut = NULL;
struct axt *axt;

if (checkOutFile != NULL)
     checkOut = mustOpen(checkOutFile, "w");
never = scaledLog(neverProb);
always = scaledLog(1.0);
makeTransitionProbs(dyno->transProbLookup);
while ((axt = axtRead(lf)) != NULL)
    {
    uglyf("%s\n", axt->tName);
    tolowers(axt->qSym);
    tolowers(axt->tSym);
    oneOrf(td, dyno, axt, checkInHash, checkOut, f);
    axtFree(&axt);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
if (optionVal("checkOut", NULL) && !optionVal("checkIn", NULL))
    errAbort("CheckOut without checkIn");
useM1 = optionExists("m1");
initSymToIx();
twinOrf(argv[1], argv[2], argv[3], optionVal("checkIn", NULL), optionVal("checkOut", NULL));
return 0;
}
