/* tTestTwo - Do statistical t-test on two sample sets and return probability that the two samples came from the same population.. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

int aCol = 1, bCol = 1;
void usage()
/* Explain usage and exit. */
{
errAbort(
  "tTestTwo - Do statistical t-test on two sample sets and return \n"
  "probability that the two samples came from the same population.\n"
  "usage:\n"
  "   tTestTwo aFile bFile\n"
  "Where aFile and bFile both contain columns of floating point numbers.\n"
  "By default the first column will be used.  Use the options below for\n"
  "other columns. The aFile and bFile can be the same\n"
  "options:\n"
  "   -aCol=N - which column to use from aFile (one based)\n"
  "   -bCol=N - which column to use from bFile (one based)\n"
  );
}

static struct optionSpec options[] = {
   {"aCol", OPTION_INT},
   {"bCol", OPTION_INT},
   {NULL, 0},
};

void readSamples(char *fileName, int colIx, double **retSamples, int *retCount)
/* Read in a column from a file of samples. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
double *samples;
int alloced = 1024;
int count = 0;
int rowSize = colIx+1;
char **row;
AllocArray(row, rowSize);
AllocArray(samples, alloced);

while (lineFileNextRow(lf, row, rowSize))
    {
    if (count >= alloced)
	 {
	 int newAlloc = alloced+alloced;
	 ExpandArray(samples, alloced, newAlloc);
	 alloced = newAlloc;
	 }
    samples[count] = lineFileNeedDouble(lf, row, colIx);
    count += 1;
    }
*retSamples = samples;
*retCount = count;
uglyf("Read %d samples from %s\n", count, fileName);
}

void calcSumSquares(double *samples, int count,
	double *retSum, double *retSumSquares)
/* Return sum and sum of squares - a handy thing for many
 * stats calculations. */
{
double sum = 0, sumSquares = 0, x;
int i;
for (i=0; i<count; ++i)
    {
    x = samples[i];
    sum += x;
    sumSquares += x*x;
    }
*retSum = sum;
*retSumSquares = sumSquares;
}

void statsCalcMeanAndSd(double *samples, int count, 
	double *retMean, double *retSd)
/* Calculate mean and standard deviation of sample. */
{
double sum = 0, sumSquares = 0;
if (count < 2)
    errAbort("Need at least two samples in statsCalcMeanAndSd");
calcSumSquares(samples, count, &sum, &sumSquares);
*retMean = sum/count;
*retSd = sqrt((sumSquares - sum*sum/count)/(count-1));
uglyf("%d samples.  Mean %f, standard dev %f\n", count, *retMean, *retSd);
}

double statsIndependentT(double *aSamples, int aCount,
	double *bSamples, int bCount)
/* Do statistical t-test on two sample sets and return probability 
 * that the two samples came from different populations (one tailed). */
{
double aSum, aSumSquares, bSum, bSumSquares;
calcSumSquares(aSamples, aCount, &aSum, &aSumSquares);
calcSumSquares(bSamples, bCount, &bSum, &bSumSquares);

double t = (aSum/aCount - bSum/bCount) /
	sqrt((aSumSquares - aSum*aSum/aCount + bSumSquares - bSum*bSum/bCount)
		       / (aCount + bCount - 2) * (1.0/aCount + 1.0/bCount));
uglyf("t %f, degrees of freedom %d\n", t, aCount+bCount-2);
return 0;	//uglyf
}

void tTestTwo(char *aFile, char *bFile)
/* tTestTwo - Do statistical t-test on two sample sets and return probability 
 * that the two samples came from the same population.. */
{
double *aSamples, *bSamples;
int aCount, bCount;
double aMean, bMean;
double aSd, bSd;
uglyTime(NULL);
readSamples(aFile, aCol-1, &aSamples, &aCount);
readSamples(bFile, bCol-1, &bSamples, &bCount);
uglyTime("Read samples");
statsCalcMeanAndSd(aSamples, aCount, &aMean, &aSd);
statsCalcMeanAndSd(bSamples, bCount, &bMean, &bSd);
uglyTime("Did SD");
statsIndependentT(aSamples, aCount, bSamples, bCount);
uglyTime("T test");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
aCol = optionInt("aCol", aCol);
bCol = optionInt("bCol", bCol);
if (aCol <= 0 || bCol <= 0)
    errAbort("aCol and bCol must be positive");
tTestTwo(argv[1], argv[2]);
return 0;
}
