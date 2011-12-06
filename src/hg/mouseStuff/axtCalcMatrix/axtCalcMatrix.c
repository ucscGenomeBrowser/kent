/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtCalcMatrix - Calculate substitution matrix and make indel histogram\n"
  "usage:\n"
  "   axtCalcMatrix files(s).axt\n"
  );
}

void addMatrix(int matrix[4][4], char *as, char *bs, int count)
/* Add alignment to substitution matrix. */
{
int i, a, b;

for (i=0; i<count; ++i)
    {
    a = ntVal[(int)as[i]];
    b = ntVal[(int)bs[i]];
    if (a >= 0 && b >= 0)
         matrix[a][b] += 1;
    }
}

void addInsert(int *hist, int histSize, char *sym, int symCount, 
	int *sumStart, int *sumExt)
/* Add inserts to histogram. */
{
char c, lastC = 0;
int i;
int insSize = 0;

/* This loop depends on the zero tag at the end. */
for (i=0; i<=symCount; ++i)
    {
    c = sym[i];
    if (c == '-')
        {
	if (lastC == '-')
	     {
	     ++insSize; 
	     *sumExt += 1;
	     }
	else
	     {
	     insSize = 1;
	     *sumStart += 1;
	     }
	}
    else
        {
	if (lastC == '-')
	     {
	     if (insSize >= histSize)
	          insSize = histSize - 1;
	     hist[insSize] += 1;
	     }
	}
    lastC = c;
    }
}

void addPerfect(struct axt *axt, int *hist, int histSize, 
	char *qSym, char *tSym, int symCount, char *bestPos)
/* Add counts of perfect runs to histogram */
{
boolean match, lastMatch = FALSE;
int startMatch = 0;
int i, matchSize;
static int best = 0;

for (i=0; i<=symCount; ++i)
    {
    if (i == symCount)
        match = FALSE;
    else
	match = (toupper(qSym[i]) == toupper(tSym[i]));
    if (match && !lastMatch)
        {
	startMatch = i;
	lastMatch = match;
	}
    else if (!match && lastMatch)
        {
	matchSize = i - startMatch;
	if (matchSize >= histSize)
	     matchSize = histSize - 1;
	hist[matchSize] += 1;
	lastMatch = match;
	if (matchSize > best)
	    {
	    best = matchSize;
	    sprintf(bestPos, "%s:%d-%d", axt->tName, axt->tStart, axt->tEnd);
	    }
	}
    }
}

void addGapless(struct axt *axt, int *hist, int histSize, 
	char *qSym, char *tSym, int symCount, char *bestPos)
/* Add counts of perfect runs to histogram */
{
boolean match, lastMatch = FALSE;
int startMatch = 0;
int i, matchSize;
static int best = 0;

for (i=0; i<=symCount; ++i)
    {
    if (i == symCount)
        match = FALSE;
    else
	match = (qSym[i] != '-' && tSym[i] != '-');
    if (match && !lastMatch)
        {
	startMatch = i;
	lastMatch = match;
	}
    else if (!match && lastMatch)
        {
	matchSize = i - startMatch;
	if (matchSize >= histSize)
	     matchSize = histSize - 1;
	hist[matchSize] += 1;
	lastMatch = match;
	if (matchSize > best)
	    {
	    best = matchSize;
	    sprintf(bestPos, "%s:%d-%d", axt->tName, axt->tStart, axt->tEnd);
	    }
	}
    }
}

void printLabeledPercent(char *label, int num, int total)
/* Print a label, absolute number, and number as percent of total. */
{
printf("%s %4.1f%% (%d)\n", label, 100.0*num/total, num);
}

void printMedianEtc(char *label, int *hist, int histSize, char *bestPos)
/* Calculate min, max, average, and median and print. */
{
int i, oneSlot, hitCount = 0;
int halfHits, hitsSoFar, median = -1; /* Stuff to calculate median. */
int biggest = 0, smallest = -1;
double total = 0, average;

/* Scan through counting up things. */
for (i=0; i<histSize; ++i)
    {
    oneSlot = hist[i];
    if (oneSlot > 0)
        {
	if (smallest < 0)
	    smallest = i;
	biggest = i;
	hitCount += oneSlot;
	total += oneSlot * i;
	}
    }
average = total / hitCount;

/* Figure out median. */
halfHits = hitCount/2;
hitsSoFar = 0;
for (i=0; i<histSize; ++i)
    {
    hitsSoFar += hist[i];
    if (hitsSoFar >= halfHits)
	{
        median = i;
	break;
	}
    }

printf("%s min %d, max %d, median %d, average %1.3f, total %4.1f\n", 
    label, smallest, biggest, median, average, total);
printf("   best at %s\n", bestPos);
}

#define maxPerfect 100000

void axtCalcMatrix(int fileCount, char *files[])
/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. */
{
int *histIns, *histDel, *histPerfect, *histGapless, *histT, *histQ;
int maxInDel = optionInt("maxInsert", 21);
static int matrix[4][4];
static char bestGapless[256], bestPerfect[256];
int i, j, total = 0;
double scale;
int fileIx;
struct axt *axt;
static int trans[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
static char *bases[4] = {"A", "C", "G", "T"};
int totalT = 0, totalMatch = 0, totalMismatch = 0, 
	tGapStart = 0, tGapExt=0, qGapStart = 0, qGapExt = 0;

AllocArray(histIns, maxInDel+1);
AllocArray(histDel, maxInDel+1);
AllocArray(histPerfect, maxPerfect+1);
AllocArray(histGapless, maxPerfect+1);
AllocArray(histT, maxInDel+1);
AllocArray(histQ, maxInDel+1);
for (fileIx = 0; fileIx < fileCount; ++fileIx)
    {
    char *fileName = files[fileIx];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    while ((axt = axtRead(lf)) != NULL)
        {
	totalT += axt->tEnd - axt->tStart;
	addMatrix(matrix, axt->tSym, axt->qSym, axt->symCount);
	addInsert(histIns, maxInDel, axt->tSym, axt->symCount,
		&tGapStart, &tGapExt);
	addInsert(histDel, maxInDel, axt->qSym, axt->symCount,
		&qGapStart, &qGapExt);
	addPerfect(axt, histPerfect, maxPerfect, 
		axt->qSym, axt->tSym, axt->symCount, bestPerfect);
	addGapless(axt, histGapless, maxPerfect, 
		axt->qSym, axt->tSym, axt->symCount, bestGapless);
	axtFree(&axt);
	}
    lineFileClose(&lf);
    }


printf("   ");
for (i=0; i<4; ++i)
    printf("%5s ", bases[i]);
printf("\n");

for (i=0; i<4; ++i)
    {
    for (j=0; j<4; ++j)
	{
	int one = matrix[i][j];
        total += matrix[i][j];
	if (i == j)
	    totalMatch += one;
	else
	    totalMismatch += one;
	}
    }
scale = 1.0 / total;

for (i=0; i<4; ++i)
    {
    int it = trans[i];
    printf(" %s", bases[i]);
    for (j=0; j<4; ++j)
        {
	int jt = trans[j];
	printf(" %5.4f", matrix[it][jt] * scale);
	}
    printf("\n");
    }
printf("\n");

for (i=1; i<21; ++i)
    {
    if (i == 20)
        printf(">=");
    printf("%2d  %6.4f%% %6.4f%%\n", i, 100.0*histIns[i]/totalT, 
    	100.0*histDel[i]/totalT);
    }

#ifdef OLD
for (i=0; i<100; i += 10)
    {
    int delSum = 0, insSum=0, perfectSum = 0, perfectBaseSum = 0;
    for (j=0; j<10; ++j)
        {
	int ix = i+j;
	insSum += histIns[ix];
	delSum += histDel[ix];
	perfectSum += histPerfect[ix];
	perfectBaseSum += histPerfect[ix] * ix;
	}
    printf("%2d to %2d:  %6.4f%% %6.4f%% %6d %7d\n", i, i+9, 
    	100.0*insSum/totalT, 100.0*delSum/totalT, perfectSum, perfectBaseSum);
    }
for (i=0; i<1000; i += 100)
    {
    int delSum = 0, insSum=0, perfectSum = 0, perfectBaseSum = 0;
    for (j=0; j<100; ++j)
        {
	int ix = i+j;
	int ins = histIns[ix];
	int del = histDel[ix];
	both = ins + del;
	insSum += ins;
	delSum += del;
	perfectSum += histPerfect[ix];
	perfectBaseSum += histPerfect[ix] * ix;
	}
    printf("%3d to %3d:  %6.4f%% %6.4f%% %6d %7d\n", i, i+99, 
    	100.0*insSum/totalT, 100.0*delSum/totalT, perfectSum, perfectBaseSum);
    }
printf(">1000  %6.4f%% %6.4f%% %6d %7d\n", 
	100.0*histIns[1000]/totalT, 100.0*histDel[1000]/totalT, histPerfect[1000],
	histPerfect[1000]*1000);
both = histIns[1000] + histDel[1000];
#endif /* OLD */

printf("\n");
printMedianEtc("perfect", histPerfect, maxPerfect, bestPerfect);
printMedianEtc("gapless", histGapless, maxPerfect, bestGapless);
printf("\n");
printLabeledPercent("totalT:    ", totalT, totalT);
printLabeledPercent("matches:   ", totalMatch, totalT);
printLabeledPercent("mismatches:", totalMismatch, totalT);
printLabeledPercent("tGapStart: ", tGapStart, totalT);
printLabeledPercent("qGapStart: ", qGapStart, totalT);
printLabeledPercent("tGapExt:   ", tGapExt, totalT);
printLabeledPercent("qGapExt:   ", qGapExt, totalT);
printLabeledPercent("baseId:    ", totalMatch, totalMatch+totalMismatch);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
if (argc < 2)
    usage();
axtCalcMatrix(argc-1, argv+1);
return 0;
}
