/* regClusterBedExpCfg - Create config file for hgBedsToBedExps from list of files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "hmmStats.h"

static char const rcsid[] = "$Id: regClusterBedExpCfg.c,v 1.1 2010/03/08 23:35:07 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regClusterBedExpCfg - Create config file for hgBedsToBedExps from list of files.\n"
  "usage:\n"
  "   regClusterBedExpCfg inputFileList output.cfg\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int commonPrefixSize(struct slName *list)
/* Return length of common prefix */
{
if (list == NULL)
    return 0;
int commonSize = strlen(list->name);
struct slName *el, *lastEl = list;
for (el = list->next; el != NULL; el = el->next)
    {
    int sameSize = countSame(el->name, lastEl->name);
    commonSize = min(sameSize, commonSize);
    lastEl = el;
    }
return commonSize;
}

int countSameAtEnd(char *a, char *b)
/* Count number of characters at end of strings that are same in each string. */
{
int count = 0;
char *aEnd = a + strlen(a);
char *bEnd = b + strlen(b);
while  (--aEnd >= a && --bEnd >= b)
    {
    if (*aEnd != *bEnd)
        break;
    ++count;
    }
return count;
}

int commonSuffixSize(struct slName *list)
/* Return length of common suffix */
{
if (list == NULL)
    return 0;
int commonSize = strlen(list->name);
struct slName *el, *lastEl = list;
for (el = list->next; el != NULL; el = el->next)
    {
    int sameSize = countSameAtEnd(el->name, lastEl->name);
    commonSize = min(sameSize, commonSize);
    lastEl = el;
    }
return commonSize;
}

void camelParseTwo(char *in, char **retA, char **retB)
/* Parse out CamelCased in into a and b.  */
{
char *s = in;
char *aStart = s;
char *bStart = NULL;
char c;
while ((c = *(++s)) != 0)
    {
    if (isupper(c))
        {
	bStart = s;
	break;
        }
    }
if (bStart == NULL)
   errAbort("Couldn't find start of second word in %s", in);
*retA = cloneStringZ(aStart, bStart - aStart);
*retB = cloneString(bStart);
}

double calcNormScoreFactor(char *fileName, int scoreCol)
/* Figure out what to multiply things by to get a nice browser score (0-1000) */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[scoreCol+1];
double sum = 0, sumSquares = 0;
int n = 0;
double minVal=0, maxVal=0;
while (lineFileRow(lf, row))
    {
    double x = sqlDouble(row[scoreCol]);
    if (n == 0)
        minVal = maxVal = x;
    if (x < minVal) minVal = x;
    if (x > maxVal) maxVal = x;
    sum += x;
    sumSquares += x*x;
    n += 1;
    }

double std = calcStdFromSums(sum, sumSquares, n);
double mean = sum/n;
double highEnd = mean + std;
if (highEnd > maxVal) highEnd = maxVal;
return 1000.0/highEnd;
}

void regClusterBedExpCfg(char *input, char *output)
/* regClusterBedExpCfg - Create config file for hgBedsToBedExps from list of files.. */
{
FILE *f = mustOpen(output, "w");
struct slName *in, *inList = readAllLines(input);
int commonPrefix = commonPrefixSize(inList);
int commonSuffix = commonSuffixSize(inList);
for (in = inList; in != NULL; in = in->next)
    {
    char *s = in->name;
    int len = strlen(s);
    char *midString = cloneStringZ(s+commonPrefix, len - commonPrefix - commonSuffix);
    char *factor, *cell;
    camelParseTwo(midString, &cell, &factor);
    fprintf(f, "%s\t%s\t", factor, cell);
    fprintf(f, "%c\t", cell[0]);
    fprintf(f, "file\t7\t");
    fprintf(f, "%g\t", calcNormScoreFactor(in->name, 7));
    fprintf(f, "%s\n", in->name);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
regClusterBedExpCfg(argv[1], argv[2]);
return 0;
}
