/* regClusterMakeTableOfTables - Make up a table of tables for regCluster program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "hmmstats.h"

static char const rcsid[] = "$Id: regClusterMakeTableOfTables.c,v 1.4 2010/05/17 02:21:36 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regClusterMakeTableOfTables - Make up a table of tables for regCluster program\n"
  "usage:\n"
  "   regClusterMakeTableOfTables fileListFile output\n"
  "Where the fileListFile is a list of narrowPeak format files.\n"
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
lineFileClose(&lf);
double std = calcStdFromSums(sum, sumSquares, n);
double mean = sum/n;
double highEnd = mean + std;
if (highEnd > maxVal) highEnd = maxVal;
return 1000.0/highEnd;
}

void makeTableFromFileNames(char *input, char *output)
/* makeTableFromFileNames - Make up a table of tables for regCluster from
 * input that is a list of file names that include metadata. */
{
FILE *f = mustOpen(output, "w");
struct slName *in, *inList = readAllLines(input);
int commonPrefix = commonPrefixSize(inList);
int commonSuffix = commonSuffixSize(inList);
int scoreColIx = 6;
for (in = inList; in != NULL; in = in->next)
    {
    fprintf(f, "%s\t0\t1\t2\tscoreColIx\t", in->name);
    fprintf(f, "%g\t", calcNormScoreFactor(in->name, scoreColIx));
    char *s = in->name;
    int len = strlen(s);
    char *midString = cloneStringZ(s+commonPrefix, len - commonPrefix - commonSuffix);
    fprintf(f, "%s\n", midString);
    freez(&midString);
    }
carefulClose(&f);
}

void regClusterMakeTableOfTables(char *input, char *output)
/* regClusterMakeTableOfTables - Make up a table of tables for regCluster program. */
{
makeTableFromFileNames(input, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
regClusterMakeTableOfTables(argv[1], argv[2]);
return 0;
}
