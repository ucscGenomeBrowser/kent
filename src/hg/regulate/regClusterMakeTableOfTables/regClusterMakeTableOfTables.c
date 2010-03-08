/* regClusterMakeTableOfTables - Make up a table of tables for regCluster program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "hmmStats.h"

static char const rcsid[] = "$Id: regClusterMakeTableOfTables.c,v 1.1 2010/03/08 23:35:08 kent Exp $";

boolean clTwo = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regClusterMakeTableOfTables - Make up a table of tables for regCluster program\n"
  "usage:\n"
  "   regClusterMakeTableOfTables fileListFile output\n"
  "options:\n"
  "   -two - assume name is camelCased with two things\n"
  );
}

static struct optionSpec options[] = {
   {"two", OPTION_BOOLEAN},
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

void regClusterMakeTableOfTables(char *input, char *output)
/* regClusterMakeTableOfTables - Make up a table of tables for regCluster program. */
{
FILE *f = mustOpen(output, "w");
struct slName *in, *inList = readAllLines(input);
int commonPrefix = commonPrefixSize(inList);
int commonSuffix = commonSuffixSize(inList);
for (in = inList; in != NULL; in = in->next)
    {
    fprintf(f, "%s\t1\t2\t3\t7\t", in->name);
    fprintf(f, "%g\t", calcNormScoreFactor(in->name, 7));
    char *s = in->name;
    int len = strlen(s);
    char *midString = cloneStringZ(s+commonPrefix, len - commonPrefix - commonSuffix);
    if (clTwo)
        {
	char *a, *b;
	camelParseTwo(midString, &a, &b);
	fprintf(f, "%s\t%s\n", a, b);
	}
    else
	fprintf(f, "%s\n", midString);
    freez(&midString);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clTwo = optionExists("two");
regClusterMakeTableOfTables(argv[1], argv[2]);
return 0;
}
