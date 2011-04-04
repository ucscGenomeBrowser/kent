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
  "   regClusterMakeTableOfTables type fileListFile output\n"
  "Where the fileListFile is a list of narrowPeak format files,\n"
  "and type is one of:\n"
  "        ans01 - Anshul's uniform peaks from Jan 2011 ENCODE freeze\n"
  "        uw01 - From UW DNase file names for hg18\n"
  "        uw02 - From UW DNase file names for hg19 as of Jan 2011 freeze\n"
  "        enh01 - From enhancer picks\n"
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

void uw01MetaOut(FILE *f, char *midString)
/* uw02MetaOut - Version of function used for UW dnase on hg18. */
{
fprintf(f, "\t%s", midString);
}

void uw02MetaOut(FILE *f, char *midString)
/* uw02MetaOut - Version of function used for UW dnase on hg19. */
{
/* At this point midString has format CellPkRep## - parse out Cell and ## */
char *pattern = "PkRep";
int patLen = strlen(pattern);
char *patPos = stringIn(pattern, midString);
if (patPos == NULL)
    errAbort("Can't find %s in %s\n", pattern, midString);
char *cell = cloneStringZ(midString, patPos - midString);
char *rep = cloneString(patPos + patLen);

/* Sometimes rep will have a 'V1' or 'V2' after it. Chop it off. */
char *v = strchr(rep, 'V');
if (v != NULL)
    *v = 0;

fprintf(f, "\t%s\t%s", cell, rep);
}

char *findKnownPrefix(char *s, char **prefixes)
/* If s starts with one of the NULL terminated array of prefixes, return that
 * prefix, otherwise return NULL. */
{
char *p;
while ((p = *prefixes++) != NULL)
    if (startsWith(p, s))
        break;
return p;
}

char *firstUpper(char *s)
/* Return pointer to first upper case letter in string */
{
for (;;)
    {
    char c = *s;
    if (c == 0)
        return NULL;
    else if (isupper(c))
        return s;
    ++s;
    }
}

void printBetween(FILE *f, char *s, char *e)
/* Print out a tab, and then from s until but not including e */
{
fputc('\t', f);
while (s < e)
    fputc(*s++, f);
}

void ans01MetaOut(FILE *f, char *midString)
/* Version of function used for Anshul's TFBS uniform peak calling ENCODE Jan 2011 freeze. */
{
char *pattern = "0_VS_";
char *patPos = stringIn(pattern, midString);
if (patPos == NULL)
    errAbort("Can't find %s in %s\n", pattern, midString);
*patPos = 0;

fprintf(f, "\twgEncode%s1", midString);
}

void oldAns01MetaOut(FILE *f, char *midString)
/* Version of function used for Anshul's TFBS uniform peak calling ENCODE Jan 2011 freeze. */
{
/* Truncate at _VS_ */
char *pattern = "AlnRep0_VS_";
char *patPos = stringIn(pattern, midString);
if (patPos == NULL)
    errAbort("Can't find %s in %s\n", pattern, midString);
*patPos = 0;

/* Get prefix from known list. */
static char *knownPrefixes[] = {"BroadHistone", "HaibTfbs", "OpenChromChip", 
				"SydhTfbs", "UchicagoTfbs", "UwTfbs", NULL};
char *prefix = findKnownPrefix(midString, knownPrefixes);
if (prefix == NULL)
    errAbort("Don't recognize composite prefix for %s", midString);
char *cellStart = midString + strlen(prefix);

/* Rely on camelCasing to find antibody */
char *abStart = firstUpper(cellStart+1);
if (abStart == NULL)
    errAbort("Can't find antibody in %s", cellStart);
char *cell = cloneStringZ(cellStart, abStart - cellStart);

/* Rely on camelCasing to find treatment */
char *treatStart = firstUpper(abStart+1);
char *ab = NULL;
if (treatStart == NULL)
    {
    if (sameString(prefix, "OpenChromChip"))
	{
        treatStart = "Std";
	ab = cloneString(abStart);
	}
    else
	errAbort("Can't find treatment in %s", midString);
    }
else
    ab = cloneStringZ(abStart, treatStart - abStart);

fprintf(f, "\t%s", cell);
fprintf(f, "\t%s", ab);
fprintf(f, "\t%s", treatStart);
fprintf(f, "\t%s", prefix);
}

void enh01MetaOut(FILE *f, char *midString)
/* Version of function used for Anshul's TFBS uniform peak calling ENCODE Jan 2011 freeze. */
{
fprintf(f, "\t%s", midString);
}


void regClusterMakeTableOfTables(char *type, char *input, char *output)
/* regClusterMakeTableOfTables - Make up a table of tables for regCluster program. */
{
FILE *f = mustOpen(output, "w");
struct slName *in, *inList = readAllLines(input);
/* Generally we'll have a bunch of file names that all start and/or end with the same
 * thing.  This loop will isolate out the bits that vary, and then call a type-specific
 * routine to output the metadata columns from the middle parts. */
int commonPrefix = commonPrefixSize(inList);
int commonSuffix = commonSuffixSize(inList);
int scoreColIx = 6;
for (in = inList; in != NULL; in = in->next)
    {
    fprintf(f, "%s\t0\t1\t2\t%d\t", in->name, scoreColIx);
    fprintf(f, "%g", calcNormScoreFactor(in->name, scoreColIx));
    char *s = in->name;
    int len = strlen(s);
    char *midString = cloneStringZ(s+commonPrefix, len - commonPrefix - commonSuffix);
    if (sameString(type, "uw01"))
	uw01MetaOut(f, midString);
    else if (sameString(type, "uw02"))
	uw02MetaOut(f, midString);
    else if (sameString(type, "ans01"))
	ans01MetaOut(f, midString);
    else if (sameString(type, "enh01"))
        enh01MetaOut(f, midString);
    else
	errAbort("Unknown type '%s' in first command line parameter.", type);
    freez(&midString);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
regClusterMakeTableOfTables(argv[1], argv[2], argv[3]);
return 0;
}
