/*	clusterClone - cluster together clone psl alignment results */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "psl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: clusterClone.c,v 1.2 2004/06/16 23:59:05 hiram Exp $";

char *chr = (char *)NULL;	/*	process the one chromosome listed */
float minCover = 50.0;		/*	percent coverage to count hit */

static struct optionSpec options[] = {
   {"chr", OPTION_STRING},
   {"minCover", OPTION_FLOAT},
   {NULL, 0},
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterClone - cluster together clone psl alignment results\n"
  "usage:\n"
  "   clusterClone [options] <file.psl>\n"
  "options:\n"
  "   -chr=<chrN> - process only this one chrN from the file\n"
  "   -minCover=P - minimum P percent coverage to count an alignment\n"
  "   -verbose=N - set verbose level N"
  );
}

struct coordEl
/* a pair of coordinates in a list */
    {
    struct coordEl *next;
    unsigned start;
    unsigned end;
    unsigned qSize;
    };

static void processResult(struct hash *chrHash, struct hash *coordHash,
	char *accName)
{
struct hashCookie cookie;
struct hashEl *hashEl;
int highCount = 0;
int secondHighest = 0;
char *highName = (char *)NULL;
char *secondHighestName = (char *)NULL;
struct hashEl *coords;
int coordCount;
int i;
unsigned lowMark = BIGNUM;
unsigned highMark = 0;
unsigned range = 0;
struct coordEl *coord;
struct coordEl **coordListPt = (struct coordEl **) NULL;
double *midPoints;
double sum;
double sumData = 0.0;
double sumSquares = 0.0;
double variance;
double stddev;
unsigned median;
unsigned mean;
unsigned querySize = 0;

/*	find highest count chrom name */
cookie=hashFirst(chrHash);
while ((hashEl = hashNext(&cookie)) != NULL)
    {
    int count = ptToInt(hashEl->val);
verbose(1,"# chr%s %d\n", hashEl->name, count);
    if (count >= highCount)
	{
	secondHighest = highCount;
	if (secondHighestName)
	    freeMem(secondHighestName);
	highCount = count;
	if (highName)
	    {
	    secondHighestName = cloneString(highName);
	    freeMem(highName);
	    }
	highName = cloneString(hashEl->name);
	}
    }
verbose(1,"# chr%s %d highest count, next: %s %d\n", highName, highCount, secondHighestName, secondHighest);
if (highCount == secondHighest)
    {
verbose(1,"# chr%s %d highest count, next: %s %d  TIE *\n", highName, highCount, secondHighestName, secondHighest);
    }

/*	for that highest count chrom, examine its coordinates, find high
 *	and low */
coords = hashLookup(coordHash, highName);

if (coords)
    coordListPt = coords->val;
else
    coordListPt = NULL;

if (coordListPt)
    coord = *coordListPt;
else
    coord = NULL;

coordCount = 0;
sum = 0.0;
sumData = 0.0;
sumSquares = 0.0;
while (coord != NULL)
    {
    double midPoint;
    if (coord->start < lowMark) lowMark = coord->start;
    if (coord->end > highMark) highMark = coord->end;
    midPoint = (double) coord->start +
	(double)(coord->end - coord->start) / 2.0;
    sum += midPoint;
    sumData += midPoint;
    sumSquares += midPoint * midPoint;
    querySize = coord->qSize;
    verbose(1,"# %d %u - %u %u\n", ++coordCount, coord->start, coord->end, coord->qSize);
    coord = coord->next;
    }
range = highMark - lowMark;
variance = 0.0;
stddev = 0.0;

if (coordCount > 0)
    {
    unsigned usStdDev;

    mean = (unsigned) (sum / coordCount);
    if (coordCount > 1)
	{
	variance = (sumSquares - ((sumData*sumData)/coordCount)) /
	    (coordCount - 1);
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    usStdDev = (unsigned) stddev;
    verbose(1,"# range: %u:%u = %u,   Mean: %u, stddev: %u\n", lowMark, highMark, range, mean, usStdDev);
    midPoints = (double *) needMem(coordCount * sizeof(double));

    coordListPt = coords->val;
    coord = *coordListPt;
    i = 0;
    while (coord != NULL)
	{
	midPoints[i++] = (double) coord->start +
	    (double)(coord->end - coord->start) / 2.0;
	coord = coord->next;
	}
    median = (unsigned) doubleMedian(coordCount, midPoints);
    verbose(1,"# Median: %u implies %u-%u\n", median,
	median - (querySize/2), median+(querySize/2));
    printf("chr%s\t%u\t%u\t%s\n", highName,
	median - (querySize/2), median+(querySize/2), accName);
    } else {
    verbose(1,"# ERROR - no coordinates found ?\n");
    }

/*	free the chrom coordinates lists	*/
cookie=hashFirst(chrHash);
while ((hashEl = hashNext(&cookie)) != NULL)
    {
    coords = hashLookup(coordHash, hashEl->name);
    if (coords)
	{
	coordListPt = coords->val;
	if (coordListPt)
	    slFreeList(coordListPt);
    	}
    }
freeMem(highName);
}

static void clusterClone(int argc, char *argv[])
{
int i;

for (i=1; i < argc; ++i)
    {
    struct lineFile *lf;
    struct psl *psl;
    unsigned tSize;
    char *prevAccName = (char *)NULL;
    struct hashEl *el;
    struct hash *chrHash = newHash(0);
    struct hash *coordHash = newHash(0);
    struct coordEl *coord;
    struct coordEl **coordListPt = (struct coordEl **) NULL;

    verbose(2,"#\tprocess: %s\n", argv[i]);
    lf=pslFileOpen(argv[i]);
    while ((struct psl *)NULL != (psl = pslNext(lf)) )
	{
	char *accName = (char *)NULL;
	char *chrName = (char *)NULL;
	int chrCount;
	double percentCoverage;

	accName = cloneString(psl->qName);
	chopSuffixAt(accName,'_');

	if ((char *)NULL == prevAccName)
		prevAccName = cloneString(accName);  /* first time */

	/*	encountered a new accession name, process the one we
 	 *	were working on
	 */
	if (differentWord(accName, prevAccName))
	    {
	    processResult(chrHash, coordHash, prevAccName);
	    freeMem(prevAccName);
	    prevAccName = cloneString(accName);
	    freeHash(&chrHash);
	    freeHash(&coordHash);
	    chrHash = newHash(0);
	    coordHash = newHash(0);
	    }

	tSize = psl->tEnd - psl->tStart;
	percentCoverage = 100.0*((double)(tSize+1)/(psl->qSize + 1));


	chrName = cloneString(psl->tName);
	stripString(chrName, "chr");
	/*	keep a hash of chrom names encountered	*/
	el = hashLookup(chrHash, chrName);
	if (el == NULL)
	    {
	    hashAddInt(chrHash, chrName, 1);
	    chrCount = 1;
	    }
	else
	    {
	    chrCount = ptToInt(el->val) + 1;
	    el->val=intToPt(chrCount);
	    }

	verbose(1,"# %s\t%u\t%u\t%u\t%.4f\t%d %s:%d-%d\n",
	    psl->qName, psl->qSize, tSize, tSize - psl->qSize,
	    percentCoverage, chrCount, psl->tName, psl->tStart, psl->tEnd);

	AllocVar(coord);
	coord->start = psl->tStart;
	coord->end = psl->tEnd;
	coord->qSize = psl->qSize;
	/*	when coverage is sufficient	*/
	if (percentCoverage > minCover)
	    {
	    /*	for each chrom name, accumulate a list of coordinates */
	    el = hashLookup(coordHash, chrName);
	    if (el == NULL)
		{
		AllocVar(coordListPt);
		hashAdd(coordHash, chrName, coordListPt);
		}
	    else
		{
		coordListPt = el->val;
		}
	    slAddHead(coordListPt,coord);
	    }

	freeMem(accName);
	freeMem(chrName);
	pslFree(&psl);
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{   
optionInit(&argc, argv, options);

if (argc < 2)
    usage();

chr = optionVal("chr", NULL);
minCover = optionFloat("minCover", 50.0);

if (chr)
    verbose(1,"#\tprocess chrom: %s\n", chr);

clusterClone(argc, argv);

return(0);
}
