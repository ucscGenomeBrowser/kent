/* faSize - print total size and total N count of FA file. */
#include "common.h"
#include "fa.h"
#include "dnautil.h"
#include "cheapcgi.h"

void usage()
/* Print usage info and exit. */
{
errAbort("faSize - print total base count in fa files.\n"
	 "usage:\n"
	 "   faSize file(s).fa\n"
	 "Command flags\n"
	 "   detailed=on       outputs name and size of each record\n");
}

struct faInfo
/* Summary info on one fa. */
   {
   struct faInfo *next;	/* Next in list. */
   char *name;		/* First word after >.  The name of seq. */
   int size;            /* Size, including N's. */
   int nCount;          /* Number of N's. */
   };

int cmpFaInfo(const void *va, const void *vb)
/* Compare two faInfo. */
{
const struct faInfo *a = *((struct faInfo **)va);
const struct faInfo *b = *((struct faInfo **)vb);
return a->size - b->size;
}


void printStats(struct faInfo **pFiList)
/* Print mean, standard deviation, min, max, and median. 
 * As a side effect this sorts list by size.*/
{
struct faInfo *fi, *minFi, *maxFi;
int elCount = slCount(*pFiList);
double total = 0, nTotal = 0;
double mean, nMean;
double sd, nSd;
double variance = 0, nVariance = 0;
double x;
int median, minSize, maxSize;

if (elCount <= 1)
    return;
for (fi = *pFiList; fi != NULL; fi = fi->next)
    {
    total += fi->size;
    nTotal += fi->nCount;
    }
mean = total/elCount;
nMean = nTotal/elCount;

for (fi = *pFiList; fi != NULL; fi = fi->next)
    {
    x = (fi->size - mean);
    variance += x*x;
    x = (fi->nCount - nMean);
    nVariance  += x*x;
    }
if (elCount > 1)
    {
    variance /= (elCount-1);
    nVariance /= (elCount-1);
    }
sd = sqrt(variance);
nSd = sqrt(nVariance);
slSort(pFiList, cmpFaInfo);
fi = slElementFromIx(*pFiList, elCount/2);
median = fi->size;
minFi = *pFiList;
maxFi = slLastEl(*pFiList);

printf("Total size: mean %2.1f sd %2.1f min %d (%s) max %d (%s) median %d\n",
	mean, sd, minFi->size, minFi->name, maxFi->size, maxFi->name, median);
printf("N count: mean %2.1f sd %2.1f\n", nMean, nSd);
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
struct lineFile *lf;
struct faInfo *fiList = NULL, *fi;
boolean detailed = cgiBoolean("detailed");
ZeroVar(&seq);

dnaUtilOpen();
for (i = 0; i<faCount; ++i)
    {
    fileName = faFiles[i];
    lf = lineFileOpen(fileName, FALSE);
    ++fileCount;
    while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
	{
	int j;
	int ns = 0;
	++seqCount;
	for (j=0; j<seq.size; ++j)
	    {
	    DNA d = seq.dna[j];
	    if (d == 'n' || d == 'N')
		++ns;
	    }
	baseCount += seq.size;
	nCount += ns;
	AllocVar(fi);
	fi->name = cloneString(seq.name);
	fi->size = seq.size;
	fi->nCount = ns;
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
    printf("%llu bases (%llu N's %llu real) in %d sequences in %d files\n",
	baseCount, nCount, baseCount - nCount, seqCount, fileCount);
    printStats(&fiList);
    }
}

int main(int argc, char *argv[])
/* Process command line . */
{
cgiSpoof(&argc, argv);
if (argc < 2)
    usage();
faSize(argv+1, argc-1);
return 0;
}
