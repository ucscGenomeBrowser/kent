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


void axtCalcMatrix(int fileCount, char *files[])
/* axtCalcMatrix - Calculate substitution matrix and make indel histogram. */
{
int *histIns, *histDel;
int maxInDel = optionInt("maxInsert", 1001);
static int matrix[4][4];
int i,j,total = 0;
double scale;
int fileIx;
struct axt *axt;
static int trans[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
static char *bases[4] = {"A", "C", "G", "T"};

AllocArray(histIns, maxInDel);
AllocArray(histDel, maxInDel);
for (fileIx = 0; fileIx < fileCount; ++fileIx)
    {
    char *fileName = files[fileIx];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    while ((axt = axtRead(lf)) != NULL)
        {
	addMatrix(matrix, axt->tSym, axt->qSym, axt->symCount);
	addInsert(histIns, maxInDel, axt->tSym, axt->symCount);
	addInsert(histDel, maxInDel, axt->qSym, axt->symCount);
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
        total += matrix[it][j];
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
    printf("%2d  %6d %6d\n", i, histIns[i], histDel[i]);
for (i=0; i<100; i += 10)
    {
    int delSum = 0, insSum=0;
    for (j=0; j<10; ++j)
        {
	delSum += histDel[i+j];
	insSum += histIns[i+j];
	}
    printf("%2d to %2d:  %6d %6d\n", i, i+9, insSum, delSum);
    }
for (i=0; i<1000; i += 100)
    {
    int delSum = 0, insSum=0;
    for (j=0; j<100; ++j)
        {
	delSum += histDel[i+j];
	insSum += histIns[i+j];
	}
    printf("%3d to %3d:  %6d %6d\n", i, i+99, insSum, delSum);
    }
printf(">1000  %6d %6d\n", histIns[1000], histDel[1000]);
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
