/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. */
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
    a = ntVal[as[i]];
    b = ntVal[bs[i]];
    if (a >= 0 && b >= 0)
         matrix[a][b] += 1;
    }
}

void addInsert(int *hist, int histSize, char *sym, int symCount)
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
	     ++insSize; 
	else
	     insSize = 1;
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

void addPerfect(int *hist, int histSize, 
	char *qSym, char *tSym, int symCount)
/* Add counts of perfect runs to histogram */
{
boolean match, lastMatch = FALSE;
int startMatch = 0;
int i, matchSize;

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
	}
    }
}

void printLabeledPercent(char *label, int num, int total)
/* Print a label, absolute number, and number as percent of total. */
{
printf("%s %7.3f%% (%d)\n", label, 100.0*num/total, num);
}

void axtCalcMatrix(int fileCount, char *files[])
/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. */
{
int *histIns, *histDel, *histPerfect, *histGapless, *histT, *histQ;
int maxInDel = optionInt("maxInsert", 1001);
static int matrix[4][4];
int i,j,both,total = 0;
double scale;
int fileIx;
struct axt *axt;
static int trans[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
static char *bases[4] = {"A", "C", "G", "T"};
int totalT = 0, totalMatch = 0, totalMismatch = 0, 
	tGapStart = 0, tGapExt=0, qGapStart = 0, qGapExt = 0;

AllocArray(histIns, maxInDel);
AllocArray(histDel, maxInDel);
AllocArray(histPerfect, maxInDel);
AllocArray(histGapless, maxInDel);
AllocArray(histT, maxInDel);
AllocArray(histQ, maxInDel);
for (fileIx = 0; fileIx < fileCount; ++fileIx)
    {
    char *fileName = files[fileIx];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    while ((axt = axtRead(lf)) != NULL)
        {
	totalT += axt->tEnd - axt->tStart;
	addMatrix(matrix, axt->tSym, axt->qSym, axt->symCount);
	addInsert(histIns, maxInDel, axt->tSym, axt->symCount);
	addInsert(histDel, maxInDel, axt->qSym, axt->symCount);
	addPerfect(histPerfect, maxInDel, axt->qSym, axt->tSym, axt->symCount);
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
    int it = trans[i];
    printf(" %s", bases[i]);
    total = 0;
    for (j=0; j<4; ++j)
	{
	int one = matrix[it][j];
        total += one;
	if (it == j)
	    totalMatch += one;
	else
	    totalMismatch += one;
	}
    if (total == 0)
        {
	for (j=0; j<4; ++j)
	    matrix[it][j] = 1;
	total = 4;
	}
    scale = 1.0/total;
    for (j=0; j<4; ++j)
        {
	int jt = trans[j];
	printf(" %5.4f", matrix[it][jt] * scale);
	}
    printf("\n");
    }
printf("\n");

for (i=1; i<10; ++i)
    printf("%2d  %6.4f%% %6.4f%% %6d %7d\n", i, 100.0*histIns[i]/totalT, 
    	100.0*histDel[i]/totalT, histPerfect[i], histPerfect[i]*i);
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
	tGapStart += ins;
	qGapStart += del;
	tGapExt += ins*(ix-1);
	qGapExt += del*(ix-1);
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
printf("\n");
printLabeledPercent("totalT:    ", totalT, totalT);
printLabeledPercent("matches:   ", totalMatch, totalT);
printLabeledPercent("mismatches:", totalMismatch, totalT);
printLabeledPercent("tGapStart: ", tGapStart, totalT);
printLabeledPercent("qGapStart: ", qGapStart, totalT);
printLabeledPercent("tGapExt:   ", tGapExt, totalT);
printLabeledPercent("qGapExt:   ", qGapExt, totalT);
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
