/* bestThreshold - Given a set of numbers for positive examples and one 
 * for negative examples, figure out threshold that best separates the 
 * two sets. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


double minDelta = 0.001;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bestThreshold - Given a set of numbers for positive examples and one for\n"
  "negative examples, figure out threshold that best separates the two sets.\n"
  "usage:\n"
  "   bestThreshold good bad\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int calcErrsAtThreshold(struct slDouble *goodList, struct slDouble *badList, double threshold)
/* Figure out how many threshold will misclassify. */
{
struct slDouble *x;
int errors = 0;
for (x=goodList; x != NULL; x = x->next)
    if (x->val < threshold)
        ++errors;
for (x=badList; x != NULL; x = x->next)
    if (x->val >= threshold)
        ++errors;
return errors;
}

struct slDouble *readNumFile(char *fileName)
/* Read in number file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
int wordCount;
struct slDouble *list = NULL, *el;
for (;;)
    {
    wordCount = lineFileChop(lf, row);
    if (wordCount == 0)
         break;
    if (wordCount > 1)
        errAbort("Just expecting one number per line in %s", fileName);
    double x = lineFileNeedDouble(lf, row, 0);
    el = slDoubleNew(x);
    slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

double calcMin(struct slDouble *list)
/* Figure min value. */
{
double minVal = list->val;
struct slDouble *el;
for (el = list->next; el != NULL; el = el->next)
    minVal = min(minVal, el->val);
return minVal;
}

double calcMax(struct slDouble *list)
/* Figure max value. */
{
double maxVal = list->val;
struct slDouble *el;
for (el = list->next; el != NULL; el = el->next)
    maxVal = max(maxVal, el->val);
return maxVal;
}

void bestThreshold(char *goodFile, char *badFile)
/* bestThreshold - Given a set of numbers for positive examples and one 
 * for negative examples, figure out threshold that best separates the 
 * two sets. */
{
struct slDouble *goodList = readNumFile(goodFile);
struct slDouble *badList = readNumFile(badFile);
if (goodList == NULL)
    errAbort("%s is empty", goodFile);
if (badList == NULL)
    errAbort("%s is empty", badFile);
int total = slCount(goodList) + slCount(badList);
double goodMin = calcMin(goodList), badMin = calcMin(badList);
double goodMax = calcMax(goodList), badMax = calcMax(badList);
double minVal = min(goodMin, badMin);
double maxVal = max(goodMax, badMax);

double threshold = (minVal + maxVal)/2;
int errors = calcErrsAtThreshold(goodList, badList, threshold);
double delta = threshold;

verbose(2, "min %g, max %g, threshold %g, errors %d, accuracy %5.3f%%\n", 
	minVal, maxVal, threshold, errors, 100 - 100.0*errors/total);
while (delta > minDelta)
    {
    double threshPlus = threshold + delta;
    double threshMinus = threshold - delta;
    int errPlus = calcErrsAtThreshold(goodList, badList, threshPlus);
    int errMinus = calcErrsAtThreshold(goodList, badList, threshMinus);
    if (errPlus < errors)
         {
	 threshold = threshPlus;
	 errors = errPlus;
	 }
    else if (errMinus < errors)
         {
	 threshold = threshMinus;
	 errors = errMinus;
	 }
    delta *= 0.5;
    verbose(2, "delta %g, threshold %g, errors %d, accuracy %5.3f%%\n", 
	    delta, threshold, errors, 100 - 100.0*errors/total);
    }
printf("Threshold %g, errors %d, total %d, accuracy %5.3f%%\n",
	threshold, errors, total, 100 - 100.0*errors/total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bestThreshold(argv[1], argv[2]);
return 0;
}
