/* faSize - print total size and total N count of FA file. */
#include "common.h"
#include "fa.h"
#include "dnautil.h"
#include "options.h"

static char const rcsid[] = "$Id: faSize.c,v 1.11 2008/04/30 23:18:13 markd Exp $";

/* command line options */
static struct optionSpec optionSpecs[] =
{
    {"detailed", OPTION_BOOLEAN},
    {"tab", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Print usage info and exit. */
{
errAbort("faSize - print total base count in fa files.\n"
	 "usage:\n"
	 "   faSize file(s).fa\n"
	 "Command flags\n"
	 "   -detailed        outputs name and size of each record\n"
         "                    has the side effect of printing nothing else\n"
         "   -tab             output statistics in a tab separated format\n");
}

struct faInfo
/* Summary info on one fa. */
   {
   struct faInfo *next;	/* Next in list. */
   char *name;		/* First word after >.  The name of seq. */
   int size;            /* Size, including N's. */
   int nCount;          /* Number of N's. */
   int lCount;          /* Number of Upper-case chars. */
   int uCount;          /* Number of Lower-case chars. */
   };

int cmpFaInfo(const void *va, const void *vb)
/* Compare two faInfo. */
{
const struct faInfo *a = *((struct faInfo **)va);
const struct faInfo *b = *((struct faInfo **)vb);
return a->size - b->size;
}


void printStats(struct faInfo **pFiList, boolean tabFmt)
/* Print mean, standard deviation, min, max, and median. 
 * As a side effect this sorts list by size.*/
{
struct faInfo *fi, *minFi, *maxFi;
int elCount = slCount(*pFiList);
double total = 0, nTotal = 0, uTotal = 0, lTotal = 0;
double mean, nMean, uMean, lMean;
double sd, nSd, uSd, lSd;
double variance = 0, nVariance = 0, uVariance = 0, lVariance = 0;
double x;
int median;

if (elCount <= 1)
    return;
for (fi = *pFiList; fi != NULL; fi = fi->next)
    {
    total += fi->size;
    nTotal += fi->nCount;
    uTotal += fi->uCount;
    lTotal += fi->lCount;
    }
mean = total/elCount;
nMean = nTotal/elCount;
uMean = uTotal/elCount;
lMean = lTotal/elCount;

for (fi = *pFiList; fi != NULL; fi = fi->next)
    {
    x = (fi->size - mean);
    variance += x*x;
    x = (fi->nCount - nMean);
    nVariance  += x*x;
    x = (fi->uCount - uMean);
    uVariance  += x*x;
    x = (fi->lCount - lMean);
    lVariance  += x*x;
    }
if (elCount > 1)
    {
    variance /= (elCount-1);
    nVariance /= (elCount-1);
    uVariance /= (elCount-1);
    lVariance /= (elCount-1);
    }
sd = sqrt(variance);
nSd = sqrt(nVariance);
uSd = sqrt(uVariance);
lSd = sqrt(lVariance);
slSort(pFiList, cmpFaInfo);
fi = slElementFromIx(*pFiList, elCount/2);
median = fi->size;
minFi = *pFiList;
maxFi = slLastEl(*pFiList);

if (tabFmt)
    {
    printf("meanSize\t%2.1f\n" ,mean);
    printf("SdSize\t%2.1f\n", sd);
    printf("minSize\t%d\n", minFi->size);
    printf("minSeqSize\t%s\n", minFi->name);
    printf("maxSize\t%d\n", maxFi->size);
    printf("maxSeqSize\t%s\n", maxFi->name);
    printf("medianSize\t%d\n", median);
    printf("nCountMean\t%2.1f\n", nMean);
    printf("nCountSd\t%2.1f\n", nSd);
    printf("upperCountMean\t%2.1f\n", uMean);
    printf("upperCountSd\t%2.1f\n", uSd);
    printf("lowerCountMean\t%2.1f\n", lMean);
    printf("lowerCountSd\t%2.1f\n", lSd);
    }
else
    {
    printf("Total size: mean %2.1f sd %2.1f min %d (%s) max %d (%s) median %d\n",
	mean, sd, minFi->size, minFi->name, maxFi->size, maxFi->name, median);
    printf("N count: mean %2.1f sd %2.1f\n", nMean, nSd);
    printf("U count: mean %2.1f sd %2.1f\n", uMean, uSd);
    printf("L count: mean %2.1f sd %2.1f\n", lMean, lSd);
    }
}

void faToDnaPC(char *poly, int size)
/* Turn any strange characters to 'n'.  Does not change size of sequence.
   Preserve case.
*/
{
int i;
dnaUtilOpen();
for (i=0; i<size; ++i)
    {
    if (ntChars[(int)poly[i]] == 0)
    	poly[i] = 'n';
    }
}


boolean faSpeedReadNextPC(struct lineFile *lf, DNA **retDna, int *retSize, char **retName)
/* Read in DNA record. */
{
char *poly;
int size;

if (!faMixedSpeedReadNext(lf, retDna, retSize, retName))
    return FALSE;
size = *retSize;
poly = *retDna;
faToDnaPC(poly, size);
return TRUE;
}


void faSize(char *faFiles[], int faCount)
/* faSize - print total size and total N count of FA files. */
{
char *fileName;
int i;
struct dnaSeq seq;
int fileCount = 0;
int seqCount = 0;
unsigned long long baseCount = 0;
unsigned long long nCount = 0;
unsigned long long uCount = 0;
unsigned long long lCount = 0;
struct lineFile *lf;
struct faInfo *fiList = NULL, *fi;
boolean detailed = optionExists("detailed");
boolean tabFmt = optionExists("tab");

ZeroVar(&seq);

dnaUtilOpen();
for (i = 0; i<faCount; ++i)
    {
    fileName = faFiles[i];
    lf = lineFileOpen(fileName, FALSE);
    ++fileCount;
    while (faSpeedReadNextPC(lf, &seq.dna, &seq.size, &seq.name))
	{
	int j;
	int ns = 0;
	int us = 0;
	int ls = 0;
	++seqCount;
	for (j=0; j<seq.size; ++j)
	    {
	    DNA d = seq.dna[j];
	    if (d == 'n' || d == 'N')
		{
		++ns;
		}
	    else
		{
		if (isupper(d)) 
		    ++us;
		if (islower(d)) 
		    ++ls;
		}
	    }
	baseCount += seq.size;
	nCount += ns;
	uCount += us;
	lCount += ls;
	AllocVar(fi);
	fi->name = cloneString(seq.name);
	fi->size = seq.size;
	fi->nCount = ns;
	fi->uCount = us;
	fi->lCount = ls;
	if (detailed)
	    {
	    printf("%s\t%d\n", seq.name, seq.size);
	    }
	slAddHead(&fiList, fi);
	}
    lineFileClose(&lf);
    }
if (!detailed)
    {
    double perCentMasked = 100.0;
    double perCentRealMasked = 100.0;
    if (baseCount > 0)
	perCentMasked = 100.0*(double)lCount/(double)baseCount;
    if ((baseCount - nCount) > 0)
	perCentRealMasked = 100.0*(double)lCount/(double)(baseCount - nCount);

    if (tabFmt)
        {
        printf("baseCount\t%llu\n", baseCount);
        printf("nBaseCount\t%llu\n", nCount);
        printf("realBaseCount\t%llu\n", baseCount - nCount);
        printf("upperBaseCount\t%llu\n", uCount);
        printf("lowerBaseCount\t%llu\n", lCount);
        printf("seqCount\t%d\n", seqCount);
        printf("fileCount\t%d\n", fileCount);
        printStats(&fiList, tabFmt);
        printf("fracMasked\t%.2f\n",perCentMasked/100.0);
        printf("fracRealMasked\t%.2f\n",perCentRealMasked/100.0);
        }
    else
        {
        printf("%llu bases (%llu N's %llu real %llu upper %llu lower) in %d sequences in %d files\n",
               baseCount, nCount, baseCount - nCount, uCount, lCount, seqCount, fileCount);
        printStats(&fiList, tabFmt);
        printf("%%%.2f masked total, %%%.2f masked real\n",
               perCentMasked, perCentRealMasked);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line . */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
faSize(argv+1, argc-1);
return 0;
}
