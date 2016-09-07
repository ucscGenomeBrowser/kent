/* Aladdin.c - main file for the small scale gene-finder. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "codebias.h"
#include "fa.h"
#include "hmmstats.h"
#include "wormdna.h"
#include "cheapcgi.h"
#include "htmshell.h"

boolean isOnWeb;

enum aStates 
/* Internal states for HMM. */
{
    aIg,                        /* Intergenic non-promoter. */
    aTxnFac,                    /* Promoter conserved element. */
    aSpace,                     /* Promoter non-conserved element. */
    
    aStart1, aStart2, aStart3,  /* Start codon. */
    aC1, aC2, aC3,              /* Regular codon. */
    aEnd1, aEnd2, aEnd3,        /* End codon. */
    
    aI1Start1, aI1Start2, aI1Start3, aI1Start4, aI1Start5,aI1Start6,         /* 3' splice site frame 1 intron. */
    aI1Loc1, aI1Loc2, aI1Loc3, aI1Loc4,   /* Mid codon (4 states for size distribution) */
    aI1End1, aI1End2, aI1End3, aI1End4, aI1End5, aI1End6, aI1End7, aI1End8, aI1End9, /* 5' splice site */
    aI1C2, aI1C3,               /* Partial codon at end. */

    aI2Start1, aI2Start2, aI2Start3, aI2Start4, aI2Start5,aI2Start6,         /* 3' splice site frame 1 intron. */
    aI2Loc1, aI2Loc2, aI2Loc3, aI2Loc4,   /* Mid codon (4 states for size distribution) */
    aI2End1, aI2End2, aI2End3, aI2End4, aI2End5, aI2End6, aI2End7, aI2End8, aI2End9, /* 5' splice site */
    aI2C3,                    /* Partial codon at end. */

    aI3Start1, aI3Start2, aI3Start3, aI3Start4, aI3Start5,aI3Start6,         /* 3' splice site frame 1 intron. */
    aI3Loc1, aI3Loc2, aI3Loc3, aI3Loc4,   /* Mid codon (4 states for size distribution) */
    aI3End1, aI3End2, aI3End3, aI3End4, aI3End5, aI3End6, aI3End7, aI3End8, aI3End9, /* 5' splice site */

    aStateCount,
};

char visStates[] =
/* User visible states of HMM. */
{
    '.',                        /* Intergenic non-promoter. */
    'P',                    /* Promoter conserved element. */
    's',                     /* Promoter non-conserved element. */
    
    'A', 'A', 'A',  /* Start codon. */
    '1', '2', '3',               /* Regular codon. */
    'Z', 'Z', 'Z',        /* End codon. */
    
    '(', '-', '-', '-', '-', '-',   /* 3' splice site frame 1 intron. */
    '-', '-', '-', '-',   /* Mid codon (4 states for size distribution) */
    '-', '-', '-', '-', '-', '-', '-', '-', ')',             /* 5' splice site */
    '2', '3',               /* Partial codon at end. */

    '(', '-', '-', '-', '-', '-',   /* 3' splice site frame 1 intron. */
    '-', '-', '-', '-',   /* Mid codon (4 states for size distribution) */
    '-', '-', '-', '-', '-', '-', '-', '-', ')',             /* 5' splice site */
    '3',                    /* Partial codon at end. */

    '(', '-', '-', '-', '-', '-',   /* 3' splice site frame 1 intron. */
    '-', '-', '-', '-',   /* Mid codon (4 states for size distribution) */
    '-', '-', '-', '-', '-', '-', '-', '-', ')',             /* 5' splice site */

};

typedef UBYTE State;    /* A state. We'll only have 256 at most for now. */


/* This lets us keep the last two columns of scores. */
static int *scoreColumns[2];
static int *curScores, *prevScores;
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


/* Transition probabilities. */
static int *transProbLookup[256];

int twiceScaledLog(double val, double scale)
/* Return scaled scaled log of val */
{
return round(scale * logScaleFactor * log(val));
}

#define unlikelyProb (1.0E-20)

static void makeTransitionProbs()
/* Allocate transition probabilities and initialize them. */
{
int i, j;
int unlikely = scaledLog(unlikelyProb);
int always = scaledLog(1.0);

/* In this loop allocate transition table and set all values to
 * something improbable, but not so improbable that the log value
 * will overflow. */
for (i=0; i<aStateCount; ++i)
    {
    transProbLookup[i] = needMem(aStateCount * sizeof(transProbLookup[i][0]) );
    for (j=0; j<aStateCount; ++j)
        transProbLookup[i][j] = unlikely;
    }

/* Make certain transitions reasonably likely. */

/* Promoter and intergenic sources. */
transProbLookup[aIg][aIg] =     scaledLog(0.998);
transProbLookup[aIg][aTxnFac] = scaledLog(0.0018);
transProbLookup[aIg][aStart1] = scaledLog(0.0002);

transProbLookup[aTxnFac][aTxnFac] = scaledLog(0.95);
transProbLookup[aTxnFac][aSpace] =  scaledLog(0.04);
transProbLookup[aTxnFac][aStart1] = scaledLog(0.01);

transProbLookup[aSpace][aSpace] =  scaledLog(0.95);
transProbLookup[aSpace][aTxnFac] = scaledLog(0.04);
transProbLookup[aSpace][aStart1] = scaledLog(0.01);

/* Coding sources. */
transProbLookup[aStart1][aStart2] = always;
transProbLookup[aStart2][aStart3] = always;
transProbLookup[aStart3][aC1] = always;

transProbLookup[aC1][aC2] =       scaledLog(0.995);
transProbLookup[aC1][aI1Start1] = scaledLog(0.005);

transProbLookup[aC2][aC3] =       scaledLog(0.995);
transProbLookup[aC2][aI2Start1] = scaledLog(0.005);

transProbLookup[aC3][aC1] =       scaledLog(0.995);
transProbLookup[aC3][aI3Start1] = scaledLog(0.0045);
transProbLookup[aC3][aEnd1] =     scaledLog(0.0005);

transProbLookup[aEnd1][aEnd2] = always;
transProbLookup[aEnd2][aEnd3] = always;
transProbLookup[aEnd3][aIg] =   always;

/* Intron Frame 1 */
transProbLookup[aI1Start1][aI1Start2] = always;
transProbLookup[aI1Start2][aI1Start3] = always;
transProbLookup[aI1Start3][aI1Start4] = always;
transProbLookup[aI1Start4][aI1Start5] = always;
transProbLookup[aI1Start5][aI1Start6] = always;
transProbLookup[aI1Start6][aI1Loc1] = always;

transProbLookup[aI1Loc1][aI1Loc1] = scaledLog(0.95);
transProbLookup[aI1Loc1][aI1Loc2] = scaledLog(0.05);

transProbLookup[aI1Loc2][aI1Loc2] = scaledLog(0.95);
transProbLookup[aI1Loc2][aI1Loc3] = scaledLog(0.05);

transProbLookup[aI1Loc3][aI1Loc3] = scaledLog(0.95);
transProbLookup[aI1Loc3][aI1Loc4] = scaledLog(0.05);

transProbLookup[aI1Loc4][aI1Loc4] = scaledLog(0.95);
transProbLookup[aI1Loc4][aI1End1] = scaledLog(0.05);

transProbLookup[aI1End1][aI1End2] = always;
transProbLookup[aI1End2][aI1End3] = always;
transProbLookup[aI1End3][aI1End4] = always;
transProbLookup[aI1End4][aI1End5] = always;
transProbLookup[aI1End5][aI1End6] = always;
transProbLookup[aI1End6][aI1End7] = always;
transProbLookup[aI1End7][aI1End8] = always;
transProbLookup[aI1End8][aI1End9] = always;

transProbLookup[aI1End9][aI1C2] = always;
transProbLookup[aI1C2][aI1C3] = always;
transProbLookup[aI1C3][aC1] = always;

/* Intron Frame 2 */
transProbLookup[aI2Start1][aI2Start2] = always;
transProbLookup[aI2Start2][aI2Start3] = always;
transProbLookup[aI2Start3][aI2Start4] = always;
transProbLookup[aI2Start4][aI2Start5] = always;
transProbLookup[aI2Start5][aI2Start6] = always;
transProbLookup[aI2Start6][aI2Loc1] = always;

transProbLookup[aI2Loc1][aI2Loc1] = scaledLog(0.95);
transProbLookup[aI2Loc1][aI2Loc2] = scaledLog(0.05);

transProbLookup[aI2Loc2][aI2Loc2] = scaledLog(0.95);
transProbLookup[aI2Loc2][aI2Loc3] = scaledLog(0.05);

transProbLookup[aI2Loc3][aI2Loc3] = scaledLog(0.95);
transProbLookup[aI2Loc3][aI2Loc4] = scaledLog(0.05);

transProbLookup[aI2Loc4][aI2Loc4] = scaledLog(0.95);
transProbLookup[aI2Loc4][aI2End1] = scaledLog(0.05);

transProbLookup[aI2End1][aI2End2] = always;
transProbLookup[aI2End2][aI2End3] = always;
transProbLookup[aI2End3][aI2End4] = always;
transProbLookup[aI2End4][aI2End5] = always;
transProbLookup[aI2End5][aI2End6] = always;
transProbLookup[aI2End6][aI2End7] = always;
transProbLookup[aI2End7][aI2End8] = always;
transProbLookup[aI2End8][aI2End9] = always;

transProbLookup[aI2End9][aI2C3] = always;
transProbLookup[aI2C3][aC1] = always;

/* Intron Frame 3 */
transProbLookup[aI3Start1][aI3Start2] = always;
transProbLookup[aI3Start2][aI3Start3] = always;
transProbLookup[aI3Start3][aI3Start4] = always;
transProbLookup[aI3Start4][aI3Start5] = always;
transProbLookup[aI3Start5][aI3Start6] = always;
transProbLookup[aI3Start6][aI3Loc1] = always;

transProbLookup[aI3Loc1][aI3Loc1] = scaledLog(0.95);
transProbLookup[aI3Loc1][aI3Loc2] = scaledLog(0.05);

transProbLookup[aI3Loc2][aI3Loc2] = scaledLog(0.95);
transProbLookup[aI3Loc2][aI3Loc3] = scaledLog(0.05);

transProbLookup[aI3Loc3][aI3Loc3] = scaledLog(0.95);
transProbLookup[aI3Loc3][aI3Loc4] = scaledLog(0.05);

transProbLookup[aI3Loc4][aI3Loc4] = scaledLog(0.95);
transProbLookup[aI3Loc4][aI3End1] = scaledLog(0.05);

transProbLookup[aI3End1][aI3End2] = always;
transProbLookup[aI3End2][aI3End3] = always;
transProbLookup[aI3End3][aI3End4] = always;
transProbLookup[aI3End4][aI3End5] = always;
transProbLookup[aI3End5][aI3End6] = always;
transProbLookup[aI3End6][aI3End7] = always;
transProbLookup[aI3End7][aI3End8] = always;
transProbLookup[aI3End8][aI3End9] = always;

transProbLookup[aI3End9][aC1] = always;
}

static void freeTransitionProbs()
/* Free transition probabilities. */
{
int i;
for (i=0; i<aStateCount; ++i)
    freez(&transProbLookup[i]);
}

void upcTraceback(int *scores, State **allStates, DNA *dna, int dnaSize, FILE *out)
/* Trace back, upper caseing coding regions as you go.  Then print out result. */
{
int i;
int maxScore = -0x3fffffff;
int maxState = 0;
int lineSize;
int maxLineSize = 50;
State *states;
State *tStates = needMem(dnaSize * sizeof(tStates[0]));

/* Find end state. */
for (i=0; i<aStateCount; ++i)
    {
    if (scores[i] > maxScore)
        {
        maxScore = scores[i];
        maxState = i;
        }
    }

for (i = dnaSize-1; i >= 0; i -= 1)
    {
    tStates[i] = visStates[maxState];
    if (maxState == aStart1 || maxState == aStart2 || maxState == aStart3
        || maxState == aC1 || maxState == aC2 || maxState == aC3
        || maxState == aI1C2 || maxState == aI1C3 || maxState == aI2C3)
        dna[i] = toupper(dna[i]);
    states = allStates[maxState];
    maxState = states[i];
    }

for (i=0; i<dnaSize; i += lineSize)
    {
    lineSize = dnaSize - i;
    if (lineSize > maxLineSize)
        lineSize = maxLineSize;
    mustWrite(out, dna+i, lineSize);
    fprintf(out, " %d\n",i+lineSize);
    mustWrite(out, tStates+i, lineSize);
    fputc('\n', out);
    fputc('\n', out);
    }
}

#define startState(curState) \
    { \
    int destState = curState; \
    int newScore = -0x3fffffff; \
    State parent = 0; \
    int oneScore; 

#define endState(curState) \
    curScores[destState] = newScore; \
    allStates[destState][dnaIx] = parent; \
    }

#define source(sourceState, emitScore) \
    if ((oneScore = transProbLookup[sourceState][destState] + emitScore + prevScores[sourceState]) > newScore) \
        { \
        newScore = oneScore; \
        parent = sourceState; \
        }

typedef int Mark0[5];
typedef int Mark1[5][5];
typedef int Mark2[5][5][5];

static inline int prob0(Mark0 m, DNA base)
/* Return probability of base considering 0 before */
{
return m[ntVal[(int)base]+1];
}

static inline int prob1(Mark1 m, DNA *dna)
/* Return probability of base considering 1 before */
{
return m[ntVal[(int)dna[-1]]+1][ntVal[(int)dna[0]]+1];
}

static inline int prob2(Mark2 m, DNA *dna)
/* Return probability of base considering 2 before */
{
return m[ntVal[(int)dna[-2]]+1][ntVal[(int)dna[-1]]+1][ntVal[(int)dna[0]]+1];
}

void makeM2Ig(Mark2 m, int unlikely)
/* Make approximation of intergenic. */
{
int i,j;

for (i=0; i<5; i++)
    {
    for (j=0; j<5; j++)
        {
        m[i][j][A_BASE_VAL+1] = m[i][j][T_BASE_VAL+1] = scaledLog(0.32); 
        m[i][j][C_BASE_VAL+1] = m[i][j][G_BASE_VAL+1] = scaledLog(0.18); 
        m[i][j][0] = unlikely; 
        }
    }
}

void makeM2End3(Mark2 m, int unlikely)
/* Make Markov approximation model for stop codon. */
{
int i,j,k;

/* Set all codons to unlikely. */
for (i=0; i<5; i++)
    {
    for (j=0; j<5; j++)
        for (k=0; k<5; ++k)
        {
        m[i][j][k] = unlikely;
        }
    }
m[T_BASE_VAL+1][A_BASE_VAL+1][A_BASE_VAL+1] = scaledLog(0.5);
m[T_BASE_VAL+1][A_BASE_VAL+1][G_BASE_VAL+1] = scaledLog(0.5);
m[T_BASE_VAL+1][G_BASE_VAL+1][A_BASE_VAL+1] = scaledLog(1.0);
}

void makeM0End2(Mark0 m, int unlikely)
/* Make approximation of m0end2 (second base in end codon). */
{
m[A_BASE_VAL+1] = scaledLog(0.666);
m[G_BASE_VAL+1] = scaledLog(0.334);
m[T_BASE_VAL+1] = m[C_BASE_VAL+1] = m[0] = unlikely;
}

void mark0Exclusive(Mark0 m, DNA base, int unlikely)
/* Make model only use base. */
{
int i;
for (i=0; i<5; ++i)
    m[i] = unlikely;
m[ntVal[(int)base]+1] = scaledLog(1.0);
}

void mark1GcRich(Mark1 m, double gcRatio, int unlikely)
/* Make a Markov level 1 model of GC richness.
 * (This is just an approximation.  It could be
 * a level 0 model and work as well, but eventually
 * I'll replace this with a better one. */
{
int i;
double atRatio = 1.0-gcRatio;
int gc = scaledLog(gcRatio*0.5);
int at = scaledLog(atRatio*0.5);
for (i=0; i<5; ++i)
    {
    m[i][0] = unlikely;
    m[i][A_BASE_VAL+1] = at;
    m[i][T_BASE_VAL+1] = at;
    m[i][G_BASE_VAL+1] = gc;
    m[i][C_BASE_VAL+1] = gc;
    }
}

void makeThreeMarkovs(Mark0 m0, Mark1 m1, Mark2 m2, char *faName)
/* Fill in zeroeth, first and second order markov models 
 * from file */
{
FILE *f = mustOpen(faName, "rb");
struct dnaSeq *seq;
int hist0[5], hist1[5][5], hist2[5][5][5];
int i, size;
DNA *dna;
int val = 0;
int lastVal = 0;
int lastLastVal = 0;
int total = 0;

zeroBytes(hist0, sizeof(hist0));
zeroBytes(hist1, sizeof(hist1));
zeroBytes(hist2, sizeof(hist2));

while ((seq = faReadOneDnaSeq(f, NULL, TRUE)) != NULL)
    {
    size = seq->size;
    dna = seq->dna;
    if (size > 0)
        {
        val = ntVal[(int)dna[0]]+1;
        hist0[val] += 1;
        }
    if (size > 1)
        {
        lastVal = val;
        val = ntVal[(int)dna[1]]+1;
        hist0[val] += 1;
        hist1[lastVal][val] += 1;
        }
    for (i=2; i<size; ++i)
        {
        lastLastVal = lastVal;
        lastVal = val;
        val = ntVal[(int)dna[i]]+1;
        hist0[val] += 1;
        hist1[lastVal][val] += 1;
        hist2[lastLastVal][lastVal][val] += 1;
        }
    freeDnaSeq(&seq);
    }
/* Total things up and save 0th order Markov. */
if (m0 != NULL)
    {
    for (i=0; i<5; ++i)
         total += hist0[i]+1;
    for (i=0; i<5; ++i)
         m0[i] = scaledLog((hist0[i] + 1.0)/total);
    }
/* Calculate and save 1st order Markov. */
if (m1 != NULL)
    {
    int j;
    for (i=0; i<5; ++i)
        {
        for (j=0; j<5; ++j)
            {
            m1[i][j] = scaledLog((hist1[i][j]+1.0)/(hist0[i]+5.0));
            }
        }
    }
/* Calculate and save 2nd order Markov. */
if (m2 != NULL)
    {
    int j,k;
    for (i=0; i<5; ++i)
        {
        for (j=0; j<5; ++j)
            {
            for (k=0; k<5; ++k)
                { 
                m2[i][j][k] = scaledLog((hist2[i][j][k]+1.0)/(hist1[i][j]+5.0));
                }
            }
        }
    }    
fclose(f);
}


void makeFrameModels(Mark2 c0, Mark2 c1, Mark2 c2, char *fileName, int unlikely)
/* Make 3 interleaved level 2 Markov models for coding regions. */
{
FILE *f = mustOpen(fileName, "rb");
struct dnaSeq *seq;
int hist0[3][5], hist1[3][5][5], hist2[3][5][5][5];
Mark1 *m[3];
int i, size;
DNA *dna;
int frame;

m[0] = c0;
m[1] = c1;
m[2] = c2;

/* Count up codon histogram for each frame */
zeroBytes(hist0, sizeof(hist0));
zeroBytes(hist1, sizeof(hist1));
zeroBytes(hist2, sizeof(hist2));
while ((seq = faReadOneDnaSeq(f, NULL, TRUE)) != NULL)
    {
    size = seq->size-3;
    dna = seq->dna;
    frame = 2;
    for (i=0; i<size; ++i)
        {
        int v1 = ntVal[(int)dna[i]]+1;
        int v2 = ntVal[(int)dna[i+1]]+1;
        int v3 = ntVal[(int)dna[i+2]]+1;
        hist0[frame][v3] += 1;
        hist1[frame][v2][v3] += 1;
        hist2[frame][v1][v2][v3] += 1;
        if (++frame >= 3)
             frame = 0;
        }
    freeDnaSeq(&seq);
    }

/* Convert counts to log-probs. */
for (frame = 0; frame < 3; ++frame)
    {
    int j,k;
    for (i=0; i<5; ++i)
        {
        for (j=0; j<5; ++j)
            {
            for (k=0; k<5; ++k)
                { 
                m[frame][i][j][k] = 
                    scaledLog((hist2[frame][i][j][k]+1.0)/(hist1[frame][i][j]+5.0));
                }
            }
        }
    }
        
/* Explicitly set stop codons to unlikely value (some noise in data set...) */
m[2][T_BASE_VAL+1][A_BASE_VAL+1][A_BASE_VAL+1] = unlikely;
m[2][T_BASE_VAL+1][A_BASE_VAL+1][G_BASE_VAL+1] = unlikely;
m[2][T_BASE_VAL+1][G_BASE_VAL+1][A_BASE_VAL+1] = unlikely;
fclose(f);
}

void makeM0(Mark0 m, double a, double c, double g, double t, int unlikely)
/* Make 0th level model based on probabilities passed in. */
{
m[A_BASE_VAL+1] = scaledLog(a);
m[C_BASE_VAL+1] = scaledLog(c);
m[G_BASE_VAL+1] = scaledLog(g);
m[T_BASE_VAL+1] = scaledLog(t);
m[0] = unlikely;
}

int reorder[4] = {A_BASE_VAL+1,C_BASE_VAL+1,G_BASE_VAL+1,T_BASE_VAL+1};

void dumpMark0(Mark0 m, char *name)
{
int i;
printf("Mark0 %s = { ", name);
for (i=0; i<5; ++i) printf("%5d,",m[i]);
printf("};\n");
}

void dumpMark1(Mark1 m, char *name)
{
int i,j;
printf("Mark1 %s = {\n", name);
for (i=0; i<5; ++i) 
    {
    printf("    {");
    for (j=0; j<5; ++j)       
        printf("%5d,", m[i][j]);
    printf("},\n");
    }
printf("};\n");
}

void dumpMark2(Mark2 m, char *name)
{
int i,j,k;
printf("Mark2 %s = {\n", name);
for (i=0; i<5; ++i) 
    {
    printf("    {\n");
    for (j=0; j<5; ++j)   
        {
        printf("        {");
        for (k=0; k<5; ++k)          
            printf("%5d,", m[i][j][k]);
        printf("},\n");
        }
    printf("    },\n");
    }
printf("};\n");
}


Mark0 m0IntronStart[6], m0IntronEnd[9];

/* Start Programatically generated tables - don't bother editing */
double aFiveSplice[6] = { 0.001, 0.001, 0.578, 0.658, 0.103, 0.195, };
double cFiveSplice[6] = { 0.001, 0.003, 0.016, 0.079, 0.034, 0.098, };
double gFiveSplice[6] = { 0.996, 0.001, 0.242, 0.096, 0.745, 0.091, };
double tFiveSplice[6] = { 0.002, 0.996, 0.164, 0.168, 0.118, 0.616, };

double aThreeSplice[9] = {0.399, 0.418, 0.275, 0.056, 0.012, 0.093, 0.032, 0.995, 0.002, };
double cThreeSplice[9] = {0.118, 0.079, 0.081, 0.034, 0.016, 0.158, 0.829, 0.001, 0.002, };
double gThreeSplice[9] = {0.072, 0.068, 0.065, 0.019, 0.005, 0.083, 0.004, 0.002, 0.995, };
double tThreeSplice[9] = {0.410, 0.435, 0.580, 0.891, 0.966, 0.667, 0.135, 0.002, 0.001, };

Mark0 m0A = { -46050,-46050,-46050,    0,-46050,};
Mark0 m0T = { -46050,    0,-46050,-46050,-46050,};
Mark0 m0G = { -46050,-46050,-46050,-46050,    0,};
Mark0 m0C2 = { -46050,-1385,-1385,-1385,-1385,};
Mark0 m0C3 = { -46050,-1385,-1385,-1385,-1385,};
Mark0 m0End2 = { -46050,-46050,-46050, -405,-1096,};

Mark1 m1Intron = {
    {-1608,-1608,-1608,-1608,-1608,},
    {-14816, -784,-1712,-1634,-1779,},
    {-14146,-1235,-1606,-1094,-1745,},
    {-14787,-1317,-1985, -785,-1968,},
    {-14085,-1280,-1622,-1084,-1704,},
};

Mark2 m2Intron = {
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-14031, -681,-1703,-1855,-1857,},
        {-13103,-1200,-1643,-1102,-1747,},
        {-13181,-1191,-1826,-1022,-1738,},
        {-13036,-1235,-1672,-1068,-1722,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-12909,-1078,-1505,-1447,-1593,},
        {-12539,-1353,-1588,-1095,-1590,},
        {-13050,-1402,-1740, -903,-1749,},
        {-12400,-1404,-1633,-1074,-1522,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-13469, -752,-1851,-1505,-1893,},
        {-12801,-1179,-1665,-1049,-1871,},
        {-14001,-1299,-2170, -663,-2314,},
        {-12818,-1208,-1682,-1076,-1853,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-12804, -953,-1739,-1470,-1562,},
        {-12462,-1265,-1482,-1145,-1753,},
        {-13001,-1451,-2022, -771,-1760,},
        {-12380,-1369,-1442,-1139,-1662,},
    },
};
Mark2 m2C1 = {
    {
        {-1608,-1608,-1608,-1608, -915,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1791,-1791,-1791,-1791,-1791,},
        {-1791,-1791,-1791,-1791,-1791,},
        {-1945,-1945,-1945,-1945,-1945,},
    },
    {
        {-1608,-1608, -915,-1608,-1608,},
        {-10055,-1208, -725,-1385, -482,},
        {-10451,-1814,-1929,-1162,-1365,},
        {-9993,-1857,-2015,-1780,-2027,},
        {-10766,-2241,-2166,-1542,-1695,},
    },
    {
        {-1608,-1608,-1608,-1608, -915,},
        {-9667,-1175, -679,-1347, -380,},
        {-9470,-1694,-1745, -829, -827,},
        {-9586,-1280,-1356, -860,-1222,},
        {-10130,-1966,-2310,-1425,-1675,},
    },
    {
        {-1791,-1791,-1791,-1098,-1791,},
        {-10127,-1121, -865,-1082, -574,},
        {-10068,-1503,-1953, -983,-1137,},
        {-10540,-1528,-1447,-1116,-1003,},
        {-10402,-1962,-1585,-1199,-1523,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-9606,-1502,-1124,-1452, -596,},
        {-9738,-1901,-1982,-1482,-1717,},
        {-10296,-1716,-1930,-1144,-1358,},
        {-10035,-2601,-2294,-1859,-2309,},
    },
};
Mark2 m2C2 = {
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-1791,-1791,-1791,-1791,-1791,},
        {-1608,-1608,-1608, -915,-1608,},
        {-1608,-1608,-1608, -915,-1608,},
        {-1608, -915, -915,-1608,-1608,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-10246,-1177,-1324,-2118,-2020,},
        {-10112,-1025,-1105, -778,-1363,},
        {-9472, -505,-1027, -641,-1628,},
        {-9483, -292, -258,  265, -240,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-10050,-1572,-1555,-1908,-2163,},
        {-9932,-1883,-2029,-1606,-1922,},
        {-10271,-1450,-1518, -871,-1988,},
        {-9647,-1258,-1217, -366,-1054,},
    },
    {
        {-1608, -915,-1608,-1608,-1608,},
        {-10417,-1305,-1408,-2202,-2098,},
        {-10078,-1370,-1431,-1119,-1761,},
        {-10719,-1389,-1775,-1200,-2152,},
        {-8984, -787, -721, -206,-1169,},
    },
    {
        {-1791,-1791,-1791,-1791,-1791,},
        {-10143,-1506,-1824,-2325,-2260,},
        {-10236,-1688,-1934,-1589,-2592,},
        {-10846,-1915,-2293,-1395,-2504,},
        {-10070,-1785,-1474, -811,-2076,},
    },
};
Mark2 m2C3 = {
    {
        {-1791,-1791,-1791,-1791,-1791,},
        {-1608,-1608,-1608,-1608, -915,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
    },
    {
        {-1791,-1791,-1791,-1791,-1791,},
        {-10555,-1683,-1379,-2397,-1582,},
        {-9625,-1518,-1998,-1258,-1741,},
        {-9464, -647, -820,-46050,-46050,},
        {-10285,-1857,-2107,-46050,-1819,},
    },
    {
        {-1791,-1791,-1791,-1791,-1791,},
        {-10231,-1096,-1440,-2302,-1807,},
        {-9677,-1495,-2299, -352,-1337,},
        {-10504,-1744,-2193,-1106,-1741,},
        {-9724,-1111,-1987,-1087,-2095,},
    },
    {
        {-1791,-1791,-1791,-1791,-1791,},
        {-9933,-1227,-1648,-2689,-1291,},
        {-10125,-1205,-1742,-1112,-1891,},
        {-9982,-1254,-1741,-1126,-1345,},
        {-10256,-1769,-2177,-1492,-2950,},
    },
    {
        { -915,-1608,-1608,-1608,-1608,},
        {-9893, -689,-1170,-1643,-1275,},
        {-9372, -154, -691, -328,-1219,},
        {-10191, -525,-1247, -440, -907,},
        {-9191, -698,-1296,  394,-1861,},
    },
};
Mark2 m2Ig = {
    {
        {-1608,-1608,-1608,-1608,-1608,}, 
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-7243, -665,-1644,-2006,-1849,},
        {-6488, -991,-1628,-1252,-1924,},
        {-6296, -993,-1877,-1166,-1819,},
        {-6228,-1116,-1684,-1224,-1653,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},  /* N */
        {-6257,-1121,-1109,-1692,-1850,},  /* T */
        {-5939,-1462,-1119,-1304,-1796,},  /* C */
        {-6202,-1334,-1637, -944,-1884,},  /* A */
        {-5612,-1335,-1605,-1080,-1642,},  /* G */
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-6660, -749,-1856,-1512,-1897,},
        {-5910,-1165,-1647,-1148,-1735,},
        {-6941,-1160,-2205, -765,-2214,},
        {-5813,-1238,-1824,-1104,-1579,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-5995,-1068,-1474,-1565,-1530,},
        {-5567,-1277,-1578,-1125,-1676,},
        {-6110,-1282,-1806, -858,-2050,},
        {-5672,-1483,-1629,-1108,-1410,},
    },
};
Mark2 m2Promo = {
    {
        {-1608,-1608,-1608,-1608,-1608,},   /* N */
        {-1608,-1608,-1608,-1608,-1608,},   /* T */
        {-1608,-1608,-1608,-1608,-1608,},   /* C */
        {-1608,-1608,-1608,-1608,-1608,},   /* A */
        {-1608,-1608,-1608,-1608,-1608,},   /* G */
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-5504, -993,-1427,-2008,-1515,},
        {-5346,-1069,-1683,-1339,-1735,},
        {-4761,-1206,-1503,-1178,-1928,},
        {-5152,-1182,-1201,-1439,-1974,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-5208,-1201,-1034,-1913,-1743,},
        {-4778,-1446,-1559,-1252,-1482,},
        {-5225,-1487,-1274,-1217,-1698,},
        {-4709,-1413,-1713,-1182,-1450,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-5152,-1145,-1302,-1687,-1597,},
        {-5003,-1196,-1912, -960,-1867,},
        {-5379,-1354,-1595,-1159,-1690,},
        {-4867,-1608,-1648, -915,-1648,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-4926,-1315,-1559,-1748,-1142,},
        {-4811,-1315,-1377,-1315,-1767,},
        {-5049,-1438,-1791,-1042,-1465,},
        {-4594,-1227,-1416,-1459,-1598,},
    },
};

Mark2 m2Space = {
    {
        {-1608,-1608,-1608,-1608,-1608,}, 
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
        {-1608,-1608,-1608,-1608,-1608,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-7243, -665,-1644,-2006,-1849,},
        {-6488, -991,-1628,-1252,-1924,},
        {-6296, -993,-1877,-1166,-1819,},
        {-6228,-1116,-1684,-1224,-1653,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},  /* N */
        {-6257,-1121,-1109,-1692,-1850,},  /* T */
        {-5939,-1462,-1119,-1304,-1796,},  /* C */
        {-6202,-1334,-1637, -944,-1884,},  /* A */
        {-5612,-1335,-1605,-1080,-1642,},  /* G */
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-6660, -749,-1856,-1512,-1897,},
        {-5910,-1165,-1647,-1148,-1735,},
        {-6941,-1160,-2205, -765,-2214,},
        {-5813,-1238,-1824,-1104,-1579,},
    },
    {
        {-1608,-1608,-1608,-1608,-1608,},
        {-5995,-1068,-1474,-1565,-1530,},
        {-5567,-1277,-1578,-1125,-1676,},
        {-6110,-1282,-1806, -858,-2050,},
        {-5672,-1483,-1629,-1108,-1410,},
    },
};

Mark2 m2End3 = {
    {
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
    },
    {
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050, -692, -692,},
        {-46050,-46050,-46050,    0,-46050,},
    },
    {
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
    },
    {
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
    },
    {
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
        {-46050,-46050,-46050,-46050,-46050,},
    },
};
/* End Programatically generated tables - resume editing */

char *amemeDir()
/* Return directory name ameme files are in. */
{
static char dir[512];
static boolean initted = FALSE;
if (!initted)
    {
    char *jkwebDir;
    initted = TRUE;
    if ((jkwebDir = getenv("JKWEB")) == NULL)
        jkwebDir = "";
    sprintf(dir, "%sameme.dir", jkwebDir);
    firstWordInFile(dir, dir, sizeof(dir));
    }
return dir;
}

void makeMarkovModels(boolean doLoad)
/* Make all the Markov models we'll use. */
{
int unlikely;
char *dir = amemeDir();
char fileName[512];
int i;

unlikely = scaledLog(unlikelyProb);
if (doLoad)
    {
    mark0Exclusive(m0A, 'a', unlikely);
    mark0Exclusive(m0T, 't', unlikely);
    mark0Exclusive(m0G, 'g', unlikely);
    makeM0(m0C2, 0.25, 0.25, 0.25, 0.25, unlikely);
    makeM0(m0C3, 0.25, 0.25, 0.25, 0.25, unlikely);
    makeM0End2(m0End2, unlikely);
    makeM2End3(m2End3, unlikely);
    sprintf(fileName, "%s%s", dir, "ceLongIntron3.fa");
    makeThreeMarkovs(NULL, m1Intron, m2Intron, fileName);
    sprintf(fileName, "%s%s", dir, "cecoding.fa");
    makeFrameModels(m2C1, m2C2, m2C3, fileName, unlikely);
    sprintf(fileName, "%s%s", dir, "ceig.fa");
    makeThreeMarkovs(NULL, NULL, m2Ig, fileName);   
    sprintf(fileName, "%s%s", dir, "ceig.fa");
    makeThreeMarkovs(NULL, NULL, m2Space, fileName);
    sprintf(fileName, "%s%s", dir, "cepromoCons.fa");
    makeThreeMarkovs(NULL, NULL, m2Promo, fileName);
    }

for (i=0; i<ArraySize(m0IntronStart); ++i)
    {
    makeM0(&m0IntronStart[i][0], aFiveSplice[i], cFiveSplice[i], gFiveSplice[i], tFiveSplice[i], unlikely);
    }
for (i=0; i<ArraySize(m0IntronEnd); ++i)
    {
    makeM0(&m0IntronEnd[i][0], aThreeSplice[i], cThreeSplice[i], gThreeSplice[i], tThreeSplice[i], unlikely);
    }
}

void dumpMarkovModels()
{
htmlHorizontalLine();
dumpMark0(m0A, "m0A");
dumpMark0(m0T, "m0T");
dumpMark0(m0G, "m0G");
dumpMark0(m0C2, "m0C2");
dumpMark0(m0C3, "m0C3");
dumpMark0(m0End2, "m0End2");

dumpMark1(m1Intron, "m1Intron");
dumpMark2(m2Intron, "m2Intron");
dumpMark2(m2C1, "m2C1");
dumpMark2(m2C2, "m2C2");
dumpMark2(m2C3, "m2C3");
dumpMark2(m2Ig, "m2Ig");
dumpMark2(m2Promo, "m2Promo");
dumpMark2(m2Space, "m2Space");
dumpMark2(m2End3, "m2End3");
htmlHorizontalLine();
}

void dynamo(DNA *dna, int dnaSize, FILE *out)
/* Run dynamic programming algorithm on HMM. */
{
State **allStates;
DNA *pos, base;
int stateCount = aStateCount;
int stateByteSize = dnaSize * sizeof(State);
int i;
int dnaIx;
int scanSize = dnaSize;
int unlikely = scaledLog(unlikelyProb);

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
prevScores[aIg] = scaledLog(0.51);
prevScores[aTxnFac] = scaledLog(0.10);
prevScores[aSpace] = scaledLog(0.25);
prevScores[aC1] = scaledLog(0.04);
prevScores[aC2] = scaledLog(0.04);
prevScores[aC3] = scaledLog(0.04);
prevScores[aI1Loc2] = scaledLog(0.04);
prevScores[aI2Loc2] = scaledLog(0.04);
prevScores[aI3Loc2] = scaledLog(0.04);

for (dnaIx=0; dnaIx<scanSize; dnaIx += 1)
    {
    pos = dna+dnaIx;
    base = pos[0];

    /* Promoter/Intergenic states. */
    startState(aIg)
        int b = prob2(m2Ig, pos);
        source(aIg, b);
        source(aEnd3, b);
    endState(aIg)

    startState(aTxnFac)
        int b = prob2(m2Promo, pos);
        source(aTxnFac, b);
        source(aIg, b);
        source(aSpace, b);
    endState(aTxnFac)

    startState(aSpace)
        int b = prob2(m2Space, pos);
        source(aSpace, b);
        source(aTxnFac, b);
    endState(aSpace)

    /* Coding states. */
    startState(aStart1)
        int b = prob0(m0A, base);
        source(aIg, b);
        source(aSpace, b);
        source(aTxnFac, b);
    endState(aStart1)

    startState(aStart2)
        int b = prob0(m0T, base);
        source(aStart1, b);
    endState(aStart2)

    startState(aStart3)
        int b = prob0(m0G, base);
        source(aStart2, b);
    endState(aStart3)

    startState(aC1)
        int b = prob2(m2C1, pos);
        source(aC3, b);
        source(aStart3, b);
        source(aI1C3, b);
        source(aI2C3, b);
        source(aI3End9, b);
    endState(aC1)

    startState(aC2)
        int b = prob2(m2C2, pos);
        source(aC1, b);
    endState(aC2)

    startState(aC3)
        int b = prob2(m2C3, pos);
        source(aC2, b);
    endState(aC3)

    startState(aEnd1)
        int b = prob0(m0T, base);
        source(aC3, b);
    endState(aEnd1)

    startState(aEnd2)
        int b = prob0(m0End2, base);
        source(aEnd1, b);
    endState(aEnd2)

    startState(aEnd3)
        int b = prob2(m2End3, pos);
        source(aEnd2, b);
    endState(aEnd3)


    /* Frame 1 Intron states */
    startState(aI1Start1)
        int b = prob0(m0IntronStart[0], base);
        source(aC1, b);
    endState(aI1Start1)

    startState(aI1Start2)
        int b = prob0(m0IntronStart[1], base);
        source(aI1Start1, b);
    endState(aI1Start2)

    startState(aI1Start3)
        int b = prob0(m0IntronStart[2], base);
        source(aI1Start2, b);
    endState(aI1Start3)

    startState(aI1Start4)
        int b = prob0(m0IntronStart[3], base);
        source(aI1Start3, b);
    endState(aI1Start4)

    startState(aI1Start5)
        int b = prob0(m0IntronStart[4], base);
        source(aI1Start4, b);
    endState(aI1Start5)

    startState(aI1Start6)
        int b = prob0(m0IntronStart[5], base);
        source(aI1Start5, b);
    endState(aI1Start6)
    
    startState(aI1Loc1)
        int b = prob1(m1Intron, pos);
        source(aI1Loc1, b);
        source(aI1Start6, b);
    endState(aI1Loc1)

    startState(aI1Loc2)
        int b = prob2(m2Intron, pos);
        source(aI1Loc2, b);
        source(aI1Loc1, b);
    endState(aI1Loc2)

    startState(aI1Loc3)
        int b = prob2(m2Intron, pos);
        source(aI1Loc3, b);
        source(aI1Loc2, b);
    endState(aI1Loc3)

    startState(aI1Loc4)
        int b = prob2(m2Intron, pos);
        source(aI1Loc4, b);
        source(aI1Loc3, b);
    endState(aI1Loc4)

    startState(aI1End1)
        int b = prob0(m0IntronEnd[0], base);
        source(aI1Loc4, b);
    endState(aI1End1)

    startState(aI1End2)
        int b = prob0(m0IntronEnd[1], base);
        source(aI1End1, b);
    endState(aI1End2)

    startState(aI1End3)
        int b = prob0(m0IntronEnd[2], base);
        source(aI1End2, b);
    endState(aI1End3)

    startState(aI1End4)
        int b = prob0(m0IntronEnd[3], base);
        source(aI1End3, b);
    endState(aI1End4)

    startState(aI1End5)
        int b = prob0(m0IntronEnd[4], base);
        source(aI1End4, b);
    endState(aI1End5)

    startState(aI1End6)
        int b = prob0(m0IntronEnd[5], base);
        source(aI1End5, b);
    endState(aI1End6)

    startState(aI1End7)
        int b = prob0(m0IntronEnd[6], base);
        source(aI1End6, b);
    endState(aI1End7)

    startState(aI1End8)
        int b = prob0(m0IntronEnd[7], base);
        source(aI1End7, b);
    endState(aI1End8)

    startState(aI1End9)
        int b = prob0(m0IntronEnd[8], base);
        source(aI1End8, b);
    endState(aI1End9)

    startState(aI1C2)
        int b = prob0(m0C2, base);
        source(aI1End9, b);
    endState(aI2C2)

    startState(aI1C3)
        int b = prob0(m0C3, base);
        source(aI1C2, b);
    endState(aI1C3)



    /* Frame 2 Intron states */
    startState(aI2Start1)
        int b = prob0(m0IntronStart[0], base);
        source(aC2, b);
    endState(aI2Start1)

    startState(aI2Start2)
        int b = prob0(m0IntronStart[1], base);
        source(aI2Start1, b);
    endState(aI2Start2)

    startState(aI2Start3)
        int b = prob0(m0IntronStart[2], base);
        source(aI2Start2, b);
    endState(aI2Start3)

    startState(aI2Start4)
        int b = prob0(m0IntronStart[3], base);
        source(aI2Start3, b);
    endState(aI2Start4)

    startState(aI2Start5)
        int b = prob0(m0IntronStart[4], base);
        source(aI2Start4, b);
    endState(aI2Start5)

    startState(aI2Start6)
        int b = prob0(m0IntronStart[5], base);
        source(aI2Start5, b);
    endState(aI2Start6)
    
    startState(aI2Loc1)
        int b = prob1(m1Intron, pos);
        source(aI2Loc1, b);
        source(aI2Start6, b);
    endState(aI2Loc1)

    startState(aI2Loc2)
        int b = prob2(m2Intron, pos);
        source(aI2Loc2, b);
        source(aI2Loc1, b);
    endState(aI2Loc2)

    startState(aI2Loc3)
        int b = prob2(m2Intron, pos);
        source(aI2Loc3, b);
        source(aI2Loc2, b);
    endState(aI2Loc3)

    startState(aI2Loc4)
        int b = prob2(m2Intron, pos);
        source(aI2Loc4, b);
        source(aI2Loc3, b);
    endState(aI2Loc4)

    startState(aI2End1)
        int b = prob0(m0IntronEnd[0], base);
        source(aI2Loc4, b);
    endState(aI2End1)

    startState(aI2End2)
        int b = prob0(m0IntronEnd[1], base);
        source(aI2End1, b);
    endState(aI2End2)

    startState(aI2End3)
        int b = prob0(m0IntronEnd[2], base);
        source(aI2End2, b);
    endState(aI2End3)

    startState(aI2End4)
        int b = prob0(m0IntronEnd[3], base);
        source(aI2End3, b);
    endState(aI2End4)

    startState(aI2End5)
        int b = prob0(m0IntronEnd[4], base);
        source(aI2End4, b);
    endState(aI2End5)

    startState(aI2End6)
        int b = prob0(m0IntronEnd[5], base);
        source(aI2End5, b);
    endState(aI2End6)

    startState(aI2End7)
        int b = prob0(m0IntronEnd[6], base);
        source(aI2End6, b);
    endState(aI2End7)

    startState(aI2End8)
        int b = prob0(m0IntronEnd[7], base);
        source(aI2End7, b);
    endState(aI2End8)

    startState(aI2End9)
        int b = prob0(m0IntronEnd[8], base);
        source(aI2End8, b);
    endState(aI2End9)

    startState(aI2C3)
        int b = prob0(m0C3, base);
        source(aI2End9, b);
    endState(aI2C3)


    /* Frame 3 Intron states */
    startState(aI3Start1)
        int b = prob0(m0IntronStart[0], base);
        source(aC3, b);
    endState(aI3Start1)

    startState(aI3Start2)
        int b = prob0(m0IntronStart[1], base);
        source(aI3Start1, b);
    endState(aI3Start2)

    startState(aI3Start3)
        int b = prob0(m0IntronStart[2], base);
        source(aI3Start2, b);
    endState(aI3Start3)

    startState(aI3Start4)
        int b = prob0(m0IntronStart[3], base);
        source(aI3Start3, b);
    endState(aI3Start4)

    startState(aI3Start5)
        int b = prob0(m0IntronStart[4], base);
        source(aI3Start4, b);
    endState(aI3Start5)

    startState(aI3Start6)
        int b = prob0(m0IntronStart[5], base);
        source(aI3Start5, b);
    endState(aI3Start6)
    
    startState(aI3Loc1)
        int b = prob1(m1Intron, pos);
        source(aI3Loc1, b);
        source(aI3Start6, b);
    endState(aI3Loc1)

    startState(aI3Loc2)
        int b = prob2(m2Intron, pos);
        source(aI3Loc2, b);
        source(aI3Loc1, b);
    endState(aI3Loc2)

    startState(aI3Loc3)
        int b = prob2(m2Intron, pos);
        source(aI3Loc3, b);
        source(aI3Loc2, b);
    endState(aI3Loc3)

    startState(aI3Loc4)
        int b = prob2(m2Intron, pos);
        source(aI3Loc4, b);
        source(aI3Loc3, b);
    endState(aI3Loc4)

    startState(aI3End1)
        int b = prob0(m0IntronEnd[0], base);
        source(aI3Loc4, b);
    endState(aI3End1)

    startState(aI3End2)
        int b = prob0(m0IntronEnd[1], base);
        source(aI3End1, b);
    endState(aI3End2)

    startState(aI3End3)
        int b = prob0(m0IntronEnd[2], base);
        source(aI3End2, b);
    endState(aI3End3)

    startState(aI3End4)
        int b = prob0(m0IntronEnd[3], base);
        source(aI3End3, b);
    endState(aI3End4)

    startState(aI3End5)
        int b = prob0(m0IntronEnd[4], base);
        source(aI3End4, b);
    endState(aI3End5)

    startState(aI3End6)
        int b = prob0(m0IntronEnd[5], base);
        source(aI3End5, b);
    endState(aI3End6)

    startState(aI3End7)
        int b = prob0(m0IntronEnd[6], base);
        source(aI3End6, b);
    endState(aI3End7)

    startState(aI3End8)
        int b = prob0(m0IntronEnd[7], base);
        source(aI3End7, b);
    endState(aI3End8)

    startState(aI3End9)
        int b = prob0(m0IntronEnd[8], base);
        source(aI3End8, b);
    endState(aI3End9)

  
    flopScores();
    }


upcTraceback(prevScores, allStates, dna, scanSize, out);

/* Clean up. */
for (i=0; i<stateCount; ++i)
    {
    freeMem(allStates[i]);
    }
freeMem(allStates);
}

void aladdin(DNA *dna, int dnaSize, FILE *out)
/* Get codon tables and call HMM thing. */
{
long start, end;
// long loadTime;  unused variable
long dynTime;

start = clock1000();
makeMarkovModels(FALSE);
end = clock1000();
// loadTime = end-start;

makeTransitionProbs();

start = clock1000();
dynamo(dna, dnaSize, out);
end = clock1000();
dynTime = end-start;

freeTransitionProbs();
//dumpMarkovModels();

//uglyf("%4.2f seconds figuring out stats from huge FA files\n", loadTime*0.001);
uglyf("%4.2f seconds running dynamic program with %d states on %d bases\n",
    dynTime*0.001, aStateCount, dnaSize);
}

void doMiddle()
/* Write the main part of an html file.  Check CGI vars, load sequence
 * and call routine to do real work. */
{
DNA *dna;
int dnaSize;

dna = cgiString("dna");
dnaFilter(dna, dna);
dnaSize = strlen(dna);
printf("<TT><PRE>");
puts("Aladdin - the toy worm gene finder. Predicted coding regions appear\n"
     "in upper case.  The symbols underneath the nucleotide sequence are:\n"
     "    .  intergenic\n"
     "    P  promoter area transcription factor binding site\n"
     "    s  spacing region between txn factor sites in promoter\n"
     "    A  start codon\n"
     "    1  first base of normal codon\n"
     "    2  second base of normal codon\n"
     "    3  third base of normal codon\n"
     "    Z  end codon\n"
     "    (  intron start\n"
     "    -  intron middle\n"
     "    )  intron end\n");
htmlHorizontalLine();
aladdin(dna, dnaSize, stdout);
}

void usage()
/* Explain usage and exit. */
{
errAbort("Aladdin - not nearly as good as Genie, but still capable of\n"
         "telling exon from intron some of the time.\n"
         "Usage:\n"
         "    aladdin in.fa outFile\n");
}

int main(int argc, char *argv[])
/* Program entry point. Check to see if on Web.  If not check command
 * line parameters and load sequence. Call routines that do rest of
 * the work. */
{
dnaUtilOpen();
isOnWeb = cgiIsOnWeb();
if (isOnWeb)
    htmShell("Aladdin Results", doMiddle, NULL);
else
    {
    FILE *out;
    struct dnaSeq *seq;

    if (argc != 3)
        usage();
    out = mustOpen(argv[2], "w");
    seq = faReadDna(argv[1]);
    aladdin(seq->dna, seq->size, out);
    }
return 0;
}
