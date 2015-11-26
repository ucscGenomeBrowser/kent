/* twinOrf2 - Predict open reading frame in cDNA given a cross species alignment. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


double utrPenalty = 0.15;
double firstBonus = 15.0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrf2 - Predict open reading frame in cDNA given a cross species alignment\n"
  "usage:\n"
  "   twinOrf2 twinOrf2.stats in.axt out.orf\n"
  "options:\n"
  "   -checkIn=seq.cds - File that says size and cds position of sequences\n"
  "   -checkOut=twinOrf.check - File that compares our cds position to real\n"
  "   -utrPenalty=N - Penalize UTR score to encourage long orfs. (default %4.3f)\n" 
  "   -firstBonus=N - Bonus for ATG being first ATG.  (default %4.3f)\n"
  , utrPenalty, firstBonus
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
double nunca = -2000; /* Probability that really never should happen */
double always;	/* Probability that should always happen */
double psuedoCount = 0.00000001;

enum aStates 
/* Internal states for HMM. */
{
    aUtr5,                      /* 5' UTR. */
    aUtr3,			/* 3' UTR. */
    
    aKoz0, aKoz1, aKoz2, aKoz3, aKoz4,  /* Kozak preamble. */
    aKoz5, aKoz6, aKoz7,        /* Start codon. */
    aKoz8, aKoz9,               /* Two bases after start */
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
    'A', 'A', 'A',  	      /* Start codon. */
    '1', '2', 		      /* More Kozak. */
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
transProbLookup[aKoz4][aKoz5] = always;
transProbLookup[aKoz5][aKoz6] = always;
transProbLookup[aKoz6][aKoz7] = always;
transProbLookup[aKoz7][aKoz8] = always;
transProbLookup[aKoz8][aKoz9] = always;
transProbLookup[aKoz9][aC3] = always;
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

#define ix2(t,q)  ((t)*7 + (q))
#define symIx2(t,q)  ix2(symToIx[(int)t],symToIx[(int)q])
/* Go from pair of letters to single letter in larger alphabet. */

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

struct markov0
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

struct markov2
/* A matrix for markov2 odds. */
    {
    double odds[7*7][7*7][7*7];
    int observations;
    };

struct markov0 *m0FromCountsInFile(struct lineFile *lf, char **initRow,
	double psuedoCt)
/* Read a base odds table. */
{
struct markov0 *o;
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
	p = atof(row[j+1]) + psuedoCt;
	o->odds[i][j] = scaledLog(p);
	}
    }
return o;
}

void dumpBaseOdds(struct markov0 *o)
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

struct markov1 *readM1Odds(struct lineFile *lf, char **initRow, double psuedoCt)
/* Read a 1st order Markov odds table. Don't convert to logs. */
{
struct  markov1 *o;
int i,i1,i2,j;
double p;
char rowLabel[4];
char *row[7*7+1];
AllocVar(o);
o->observations = atoi(initRow[1]);
for (i1=0; i1<7; ++i1)
    {
    for (i2=0; i2<7; ++i2)
	{
	i = i1*7 + i2;
	lineFileRow(lf, row);
	sprintf(rowLabel, "%c/%c", ixToSym[i1], ixToSym[i2]);
	if (!sameWord(rowLabel, row[0]))
	     errAbort("Expecting %s line %d of %s\n", 
		rowLabel, lf->lineIx, lf->fileName);
	for (j=0; j<7*7; ++j)
	    {
	    p = atof(row[j+1]) + psuedoCt;
	    if (p <= 0) p = neverProb;
	    o->odds[i][j] = p;
	    }
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

struct markov1 *m1FromCountsInFile(struct lineFile *lf, char **initRow,
	double psuedoCt)
/* Read in counts from file and convert to markov model. */
{
struct markov1 *o = readM1Odds(lf, initRow, psuedoCt);
m1Normalize(o);
return o;
}

struct markov2 *readM2Odds(struct lineFile *lf, char **initRow, double psuedoCt)
/* Read a 1st order Markov odds table. Don't convert to logs. */
{
struct  markov2 *o;
int i,j,k,i1,j1,i2,j2;
double p;
char rowLabel[6];
char *row[7*7+1];
AllocVar(o);
o->observations = atoi(initRow[1]);
for (i1=0; i1<7; ++i1)
    {
    for (i2=0; i2<7; ++i2)
	{
	i = i1*7 + i2;
	for (j1=0; j1<7; ++j1)
	    {
	    for (j2=0; j2<7; ++j2)
		{
		j = j1*7 + j2;
		lineFileRow(lf, row);
		sprintf(rowLabel, "%c%c/%c%c", ixToSym[i1], ixToSym[j1],
		    ixToSym[i2], ixToSym[j2]);
		if (!sameWord(rowLabel, row[0]))
		     errAbort("Expecting %s line %d of %s\n", 
		     	rowLabel, lf->lineIx, lf->fileName);
		for (k=0; k<7*7; ++k)
		    {
		    p = atof(row[k+1]) + psuedoCt;
		    o->odds[i][j][k] = p;
		    }
		}
	    }
	}
    }
return o;
}

void m2Normalize(struct markov2 *o)
/* Normalize rows of markov2 matrix  and convert to logs. */
{
int i,j,k;

for (i=0; i<7*7; ++i)
   {
   for (j=0; j<7*7; ++j)
       {
       double rowTotal = 0;
       for (k=0; k<7*7; ++k)
	   rowTotal += o->odds[i][j][k];
       if (rowTotal > 0)
	   {
	   for (k=0; k<7*7; ++k)
	       o->odds[i][j][k] = scaledLog(o->odds[i][j][k]/rowTotal);
	   }
       }
   }
}

struct markov2 *m2FromCountsInFile(struct lineFile *lf, char **initRow,
	double psuedoCt)
/* Read in counts from file and convert to markov model. */
{
struct markov2 *o = readM2Odds(lf, initRow, psuedoCt);
m2Normalize(o);
return o;
}

static char *stopCodons[] = {"TAA", "TAG", "TGA"};

void tweakStopCodons(struct markov2 *o, boolean keep)
/* Set any target sequence involving stop codons to very low probability */
{
int i1,i2,j1,j2,k1,k2,i,j,k;
int s;
for (i1=0; i1<7; ++i1)
    {
    for (i2=0; i2<7; ++i2)
        {
	i = ix2(i1,i2);
	for (j1 = 0; j1<7; ++j1)
	    {
	    for (j2 = 0; j2<7; ++j2)
	        {
		j = ix2(j1,j2);
		for (k1=0; k1<7; ++k1)
		    {
		    for (k2=0; k2<7; ++k2)
		        {
			boolean kill = FALSE;
			k = ix2(k1, k2);
			for (s=0; s<ArraySize(stopCodons); ++s)
			    {
			    char *stop = stopCodons[s];
			    kill |= (symToIx[(int)stop[0]] == i1
			     	     && symToIx[(int)stop[1]] == j1
			     	     && symToIx[(int)stop[2]] == k1);
			    }
			if (keep)
			    kill = !kill;
			if (kill)
			    o->odds[i][j][k] = nunca;
			}
		    }
		}
	    }
	}
    }
}

void killStopCodons(struct markov2 *o)
/* Set any target sequence involving stop codons to very low probability */
{
tweakStopCodons(o, FALSE);
}

void keepOnlyStops(struct markov2 *o)
/* Get rid of everything that's not 'tag' or 'tga', or 'taa'
 * in the target. */
{
tweakStopCodons(o, TRUE);
}

void killInsertsInCodons(struct markov2 *o)
/* Get rid of inserts in the target. */
{
int i1,i2,j1,j2,k1,k2,i,j,k;
int dash = symToIx[(int)'-'];
for (i1=0; i1<7; ++i1)
    {
    for (i2=0; i2<7; ++i2)
        {
	i = ix2(i1,i2);
	for (j1 = 0; j1<7; ++j1)
	    {
	    for (j2 = 0; j2<7; ++j2)
	        {
		j = ix2(j1,j2);
		for (k1=0; k1<7; ++k1)
		    {
		    for (k2=0; k2<7; ++k2)
		        {
			k = ix2(k1, k2);
			if (i1 == dash || j1 == dash || k1 == dash)
		             o->odds[i][j][k] = nunca;
			}
		    }
		}
	    }
	}
    }
}

struct trainingData
/* Stuff loaded from training stats file. */
    {
    struct trainingData *next;
    struct markov0 *m0Utr5;	/* 5' UTR */
    struct markov1  *m1Utr5;	/* Markov 1st order of 5' UTR */
    struct markov2  *m2Utr5;	/* Markov 2nd order of 5' UTR */
    struct markov0 *m0Utr3;	/* 3' UTR */
    struct markov1  *m1Utr3;	/* Markov 1st order of 3' UTR */
    struct markov2  *m2Utr3;	/* Markov 2nd order of 3' UTR */
    struct markov0 *m0Kozak[10]; /* Kozak (start codon) consensus */
    struct markov1 *m1Kozak[10]; /* Kozak (start codon) consensus */
    struct markov2 *cod1;	/* 1st codon position */
    struct markov2 *cod2;	/* 2nd codon position */
    struct markov2 *cod3;	/* 3rd codon position */
    struct markov2 *stop;	/* stop codon position */
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
     if (sameString(type, "c1_utr5"))
         td->m0Utr5 = m0FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "c2_utr5"))
         td->m1Utr5 = m1FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "c3_utr5"))
         td->m2Utr5 = m2FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "c1_utr3"))
         td->m0Utr3 = m0FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "c2_utr3"))
         td->m1Utr3 = m1FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "c3_utr3"))
         td->m2Utr3 = m2FromCountsInFile(lf, row, psuedoCount);
     else if (startsWith("c1_kozak[", type))
         {
	 int ix = atoi(type + strlen("c1_kozak["));
	 ix += 5;
	 if (ix < 0 || ix >= 10)
	     errAbort("kozak out of range line %d of %s", lf->lineIx, lf->fileName);
	 td->m0Kozak[ix] = m0FromCountsInFile(lf, row, 0);
	 }
     else if (startsWith("c2_kozak[", type))
         {
	 int ix = atoi(type + strlen("c2_kozak["));
	 ix += 5;
	 if (ix < 0 || ix >= 10)
	     errAbort("kozak out of range line %d of %s", lf->lineIx, lf->fileName);
	 td->m1Kozak[ix] = m1FromCountsInFile(lf, row, 0);
	 }
     else if (sameString(type, "cod1"))
         td->cod1 = m2FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "cod2"))
         td->cod2 = m2FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "cod3"))
         td->cod3 = m2FromCountsInFile(lf, row, psuedoCount);
     else if (sameString(type, "stop"))
	 {
         td->stop = readM2Odds(lf, row, psuedoCount);
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
lineFileClose(&lf);
if (td->m0Utr3 == NULL)
    errAbort("Missing 'c1_utr3' record from %s\n", fileName);
if (td->m1Utr3 == NULL)
    errAbort("Missing 'c2_utr3' record from %s\n", fileName);
if (td->m2Utr3 == NULL)
    errAbort("Missing 'c3_utr3' record from %s\n", fileName);
if (td->m0Utr5 == NULL)
    errAbort("Missing 'c1_utr5' record from %s\n", fileName);
if (td->m1Utr5 == NULL)
    errAbort("Missing 'c2_utr5' record from %s\n", fileName);
if (td->m2Utr5 == NULL)
    errAbort("Missing 'c3_utr5' record from %s\n", fileName);
if (td->cod1 == NULL)
    errAbort("Missing 'cod1' record from %s\n", fileName);
if (td->cod2 == NULL)
    errAbort("Missing 'cod2' record from %s\n", fileName);
if (td->cod3 == NULL)
    errAbort("Missing 'cod3' record from %s\n", fileName);
if (td->stop == NULL)
    errAbort("Missing 'stop' record from %s\n", fileName);
for (i=0; i<10; ++i)
    {
    if (td->m0Kozak[i] == NULL)
	errAbort("Missing 'c1_kozak[%d]' record from %s\n", i-5, fileName);
    if (td->m1Kozak[i] == NULL)
	errAbort("Missing 'c2_kozak[%d]' record from %s\n", i-5, fileName);
    }
killStopCodons(td->cod3);
keepOnlyStops(td->stop);
killInsertsInCodons(td->cod1);
killInsertsInCodons(td->cod2);
killInsertsInCodons(td->cod3);
return td;
}


double probM0(struct markov0 *o, char q, char t)
/* Find probability of simple q/t pair. */
{
double x;
x = o->odds[symToIx[(int)t]][symToIx[(int)q]];
// uglyf("                 probM0(%c/%c) = %f\n", t, q, x);
return x;
}

double probM1(struct markov1 *o, char *q, char *t)
/* Find probability of 1st order markov hit. */
{
double x;
x = o->odds[symIx2(t[0],q[0])][symIx2(t[1],q[1])];
// uglyf("                 probM1(%c%c/%c%c) = %f\n", t[0], t[1], q[0], q[1], x);
return x;
}

double probM2(struct markov2 *o, char *q, char *t)
/* Find probability of 2st order markov hit. */
{
double x;
x = o->odds[symIx2(t[0],q[0])][symIx2(t[1],q[1])][symIx2(t[2],q[2])];
// uglyf("                 probM2(%c%c%c/%c%c%c) = %f\n", t[0], t[1], t[2], q[0], q[1], q[2], x);
return x;
}

double probUpToM2(struct markov0 *m0, struct markov1 *m1, struct markov2 *m2,
	char *q, char *t, int curIx)
/* Return probability from markov 0 if on 1st base,  markov 1 if on 2nd,
 * and otherwise markov 2 */
{
if (curIx == 0)
    return probM0(m0, *q, *t);
else if (curIx == 1)
    return probM1(m1, q, t);
else
    return probM2(m2, q+curIx-2, t+curIx-2);
}

double probUpToM1(struct markov0 *m0, struct markov1 *m1,
	char *q, char *t, int curIx)
/* Return probability from markov 0 if on 1st base,  
 * and otherwiser markov 1 */
{
if (curIx == 0)
    return probM0(m0, *q, *t);
else
    return probM1(m1, q+curIx-1, t+curIx-1);
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

double codonProb(struct markov2 *o, char *qSym, char *tSym, int symIx)
/* Return probability of last codon. */
{
static char qCodon[4], tCodon[4];
if (!getPrevNonInsert(qSym, tSym, symIx, qCodon, tCodon, 3))
    return never;
// uglyf("%c%c%c %c%c%c  vs %c%c%c %c%c%c\n", tSym[symIx-2], tSym[symIx-1], tSym[symIx], qSym[symIx-2], qSym[symIx-1], qSym[symIx], tCodon[0], tCodon[1], tCodon[2], qCodon[0], qCodon[1], qCodon[2]);
return probM2(o, qCodon, tCodon);
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

    // uglyf(" %d(%c) from %d(%c) score %f\n", destState, visStates[destState], parent, visStates[parent], newScore);


#define source(sourceState, emitScore) \
    if ((oneScore = dyno->transProbLookup[sourceState][destState] + emitScore + dyno->prevScores[sourceState]) > newScore) \
        { \
        newScore = oneScore; \
        parent = sourceState; \
        }


double fullDynamo(struct trainingData *td, struct dynoData *dyno, struct axt *axt, 
	struct hash *checkInHash, FILE *checkOut, FILE *f)
/* Run dynamic programming algorithm on HMM. Return score. */
{
double score = 0;
char *qSym = axt->qSym;
char *tSym = axt->tSym;
int symCount = axt->symCount;
State **allStates;
char *q, *t, tc;
// char  qc;  unused
int stateCount = dyno->stateCount;
int stateByteSize = symCount * sizeof(State);
int i;
int symIx;
int scanSize = symCount;
double tIsIns;
// double tNotIns;  unused
boolean firstAtg = TRUE;
// boolean firstKozAtg = TRUE;

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
dyno->prevScores[aUtr5] = scaledLog(0.1);
dyno->prevScores[aKoz0] = scaledLog(0.1);
dyno->prevScores[aKoz1] = scaledLog(0.1);
dyno->prevScores[aKoz2] = scaledLog(0.1);
dyno->prevScores[aKoz3] = scaledLog(0.1);
dyno->prevScores[aKoz4] = scaledLog(0.1);
dyno->prevScores[aKoz5] = scaledLog(0.1);
dyno->prevScores[aKoz6] = scaledLog(0.1);
dyno->prevScores[aKoz7] = scaledLog(0.1);
dyno->prevScores[aKoz8] = scaledLog(0.1);
dyno->prevScores[aKoz9] = scaledLog(0.1);
dyno->prevScores[aC1] = scaledLog(0.1);
dyno->prevScores[aC2] = scaledLog(0.1);
dyno->prevScores[aC3] = scaledLog(0.1);
dyno->prevScores[aUtr3] = scaledLog(0.1);

for (symIx=0; symIx<scanSize; symIx += 1)
    {
    q = qSym + symIx;
    t = tSym + symIx;
//    qc = *q;  unused
    tc = *t;
    // uglyf("%d %c %c\n", symIx, tc, qc);
    if (tc == '-')
        {
//	tNotIns = never;  unused
	tIsIns = always;
	}
    else
        {
//	tNotIns = always;  unused
	tIsIns = never;
	}

    /* Utr5 state. */
    startState(aUtr5)
        double b = probUpToM2(td->m0Utr5, td->m1Utr5, td->m2Utr5, 
		qSym, tSym, symIx);
	b -= utrPenalty;
	source(aUtr5, b);
    endState(aUtr5)

    /* Near start codon states. */
    startState(aKoz0)
	double b = probUpToM1(td->m0Kozak[0], td->m1Kozak[0], 
		qSym, tSym, symIx);
        source(aUtr5, b);
    endState(aKoz0)

    startState(aKoz1)
	double b = probUpToM1(td->m0Kozak[1], td->m1Kozak[1], 
		qSym, tSym, symIx);
        source(aKoz0, b);
    endState(aKoz1)

    startState(aKoz2)
	double b = probUpToM1(td->m0Kozak[2], td->m1Kozak[2], 
		qSym, tSym, symIx);
        source(aKoz1, b);
    endState(aKoz2)

    startState(aKoz3)
	double b = probUpToM1(td->m0Kozak[3], td->m1Kozak[3], 
		qSym, tSym, symIx);
        source(aKoz2, b);
    endState(aKoz3)

    startState(aKoz4)
	double b = probUpToM1(td->m0Kozak[4], td->m1Kozak[4], 
		qSym, tSym, symIx);
        source(aKoz3, b);
    endState(aKoz4)

    startState(aKoz5)
	double b = probUpToM1(td->m0Kozak[5], td->m1Kozak[5], 
		qSym, tSym, symIx);
        source(aKoz4, b);
    endState(aKoz5)

    startState(aKoz6)
	double b = probUpToM1(td->m0Kozak[6], td->m1Kozak[6], 
		qSym, tSym, symIx);
        source(aKoz5, b);
    endState(aKoz6)

    startState(aKoz7)
	/* the G in ATG is here */
	double b = probUpToM1(td->m0Kozak[7], td->m1Kozak[7], 
		qSym, tSym, symIx);
	char qAtg[4], tAtg[4], qKoz[7], tKoz[7];
	boolean isAtg = FALSE, isKoz = FALSE;

	qAtg[3] = tAtg[3] = qKoz[6] = tKoz[6] = 0;
	if (getPrevNonInsert(qSym, tSym, symIx, qKoz, tKoz, 6))
	    {
	    isAtg = (tKoz[3] == 'a' && tKoz[4] == 't' && tKoz[5] == 'g');
	    if (isAtg)
		isKoz = (tKoz[0] == 'a' || tKoz[0] == 'g');
	    }
	else if (getPrevNonInsert(qSym, tSym, symIx, qAtg, tAtg, 3))
	    {
	    isAtg = (tAtg[0] == 'a' && tAtg[1] == 't' && tAtg[2] == 'g');
	    }
	if (isAtg)
	    {
	    if (firstAtg)
	        {
		firstAtg = FALSE;
		b += firstBonus;
		}
	    if (isKoz)
	        {
//		firstKozAtg = FALSE;  unused
		b += firstBonus;
		}
	    }
	else
	    b = never;
        source(aKoz6, b);
    endState(aKoz7)

    startState(aKoz8)
	double b = probUpToM1(td->m0Kozak[8], td->m1Kozak[8], 
		qSym, tSym, symIx);
        source(aKoz7, b);
    endState(aKoz8)

    startState(aKoz9)
	double b = probUpToM1(td->m0Kozak[9], td->m1Kozak[9], 
		qSym, tSym, symIx);
        source(aKoz8, b);
    endState(aKoz9)

    /* Coding main states */
    startState(aC1)
	double b = codonProb(td->cod1, qSym, tSym, symIx);
	if (tc == '-')
	   b = never;
        source(aC3, b);	
	source(aInsC3, b);
	source(aI3C3, b);
    endState(aC1)

    startState(aC2)
	double b = codonProb(td->cod2, qSym, tSym, symIx);
	if (tc == '-')
	   b = never;
        source(aC1, b);
	source(aInsC1, b);
	source(aI3C1, b);
    endState(aC2)

    startState(aC3)
	double b = codonProb(td->cod3, qSym, tSym, symIx);
	if (tc == '-')
	   b = never;
        source(aKoz9, b);
	source(aC2, b);
	source(aInsC2, b);
	source(aI3C2, b);
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
	    b = probM2(td->stop, q-2, t-2);
	    // uglyf("  stop prob %c%c%c/%c%c%c = %f\n", t[-2], t[-1], t[0], q[-2], q[-1], q[0],  b);
	    }
        source(aStop2, b);
    endState(aStop3)

    /* UTR states. */
    startState(aUtr3)
        double b = probUpToM2(td->m0Utr3, td->m1Utr3, td->m2Utr3, 
		qSym, tSym, symIx);
	b -= utrPenalty;
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


void twinOrf2(char *statsFile, char *axtFile, char *outFile, 
	char *checkInFile, char *checkOutFile)
/* twinOrf2 - Predict open reading frame in cDNA given a cross species 
 * alignment. */
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
utrPenalty = optionFloat("utrPenalty", utrPenalty);
firstBonus = optionFloat("firstBonus", firstBonus);
initSymToIx();
twinOrf2(argv[1], argv[2], argv[3], optionVal("checkIn", NULL), optionVal("checkOut", NULL));
return 0;
}

