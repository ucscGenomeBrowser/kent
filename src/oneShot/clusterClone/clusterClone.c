/*	clusterClone - cluster together clone psl alignment results */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "psl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: clusterClone.c,v 1.3 2004/06/17 21:40:33 hiram Exp $";

float minCover = 50.0;		/*	percent coverage to count hit */

static struct optionSpec options[] = {
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
  "   -minCover=P - minimum P percent coverage to count an alignment (default 50.0)\n"
  "   -verbose=N - set verbose level N (3: everything, 2: important stuff)"
  );
}

struct coordEl
/* a pair of coordinates in a list */
    {
    struct coordEl *next;
    char * name;
    unsigned start;
    unsigned end;
    unsigned qSize;
    int strand;	/* 1 = +   0 = -	*/
    };

static void processResult(struct hash *chrHash, struct hash *coordHash,
	char *accName, unsigned querySize)
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
int strandSum;

/*	find highest count chrom name */
cookie=hashFirst(chrHash);
while ((hashEl = hashNext(&cookie)) != NULL)
    {
    int count = ptToInt(hashEl->val);
verbose(2,"# chr%s %d\n", hashEl->name, count);
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
verbose(2,"# chr%s %d highest count, next: %s %d\n", highName, highCount, secondHighestName, secondHighest);
if (highCount == secondHighest)
    {
verbose(2,"# chr%s %d highest count, next: %s %d  TIE *\n", highName, highCount, secondHighestName, secondHighest);
    }

/*	for that highest count chrom, examine its coordinates, find high
 *	and low */
coords = hashLookup(coordHash, highName);

if (coords) coordListPt = coords->val;
else coordListPt = NULL;

if (coordListPt) coord = *coordListPt;
else coord = NULL;

coordCount = 0;
sum = 0.0;
sumData = 0.0;
sumSquares = 0.0;
strandSum = 0;
while (coord != NULL)
    {
    double midPoint;
    if (coord->start < lowMark) lowMark = coord->start;
    if (coord->end > highMark) highMark = coord->end;
    midPoint = (double) coord->start +
	(double)(coord->end - coord->start) / 2.0;
    sum += midPoint;
    strandSum += coord->strand;
    sumData += midPoint;
    sumSquares += midPoint * midPoint;
    verbose(2,"# %d %s %u - %u %u %c\n", ++coordCount, coord->name, coord->start, coord->end, coord->qSize, (coord->strand == 1) ? '+' : '-');
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
    verbose(2,"# range: %u:%u = %u,   Mean: %u, stddev: %u\n", lowMark, highMark, range, mean, usStdDev);
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
    verbose(2,"# qSize: %u, Median: %u implies %u-%u %s\n", querySize, median,
	median - (querySize/2), median+(querySize/2), accName);
    printf("chr%s\t%u\t%u\t%s\t%c\n", highName,
	median - (querySize/2), median+(querySize/2), accName,
	strandSum > (coordCount/2) ? '+' : '-');
    freeMem(midPoints);
    }
else
    {
    printf("# ERROR - no coordinates found ? %s\n", highName);
    }

/*	free the chrom coordinates lists	*/
cookie=hashFirst(chrHash);
while ((hashEl = hashNext(&cookie)) != NULL)
    {
    coords = hashLookup(coordHash, hashEl->name);

    if (coords) coordListPt = coords->val;
    else coordListPt = NULL;

    if (coordListPt) coord = *coordListPt;
    else coord = NULL;

    while (coord != NULL)
	{
	freeMem(coord->name);
	coord = coord->next;
    	}

    if (coordListPt)
	slFreeList(coordListPt);
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
    char *prevAccPart = (char *)NULL;
    char *prevAccName = (char *)NULL;
    struct hashEl *el;
    struct hash *chrHash = newHash(0);
    struct hash *coordHash = newHash(0);
    struct coordEl *coord;
    struct coordEl **coordListPt = (struct coordEl **) NULL;
    unsigned querySize = 0;
    int partCount = 0;

    verbose(2,"#\tprocess: %s\n", argv[i]);
    lf=pslFileOpen(argv[i]);
    while ((struct psl *)NULL != (psl = pslNext(lf)) )
	{
	char *accName = (char *)NULL;
	char *chrName = (char *)NULL;
	int chrCount;
	double percentCoverage;

	accName = cloneString(psl->qName);
	if ((char *)NULL == prevAccPart)
	    {
	    prevAccPart = cloneString(psl->qName);  /* first time */
	    querySize = psl->qSize;
	    }
	chopSuffixAt(accName,'_');

	if ((char *)NULL == prevAccName)
		prevAccName = cloneString(accName);  /* first time */

	/*	encountered a new accession name, process the one we
 	 *	were working on
	 */
	if (differentWord(accName, prevAccName))
	    {
	    if (partCount > 0)
		processResult(chrHash, coordHash, prevAccName, querySize);
	    else
		printf("# ERROR - no coordinates found ? %s\n", prevAccName);
	    freeMem(prevAccName);
	    prevAccName = cloneString(accName);
	    freeHash(&chrHash);
	    freeHash(&coordHash);
	    chrHash = newHash(0);
	    coordHash = newHash(0);
	    querySize = 0;
	    partCount = 0;
	    }

	tSize = psl->tEnd - psl->tStart;
	percentCoverage = 100.0*((double)(tSize+1)/(psl->qSize + 1));
	if (differentWord(psl->qName, prevAccPart))
	    {
	    querySize += psl->qSize;
	    freeMem(prevAccPart);
	    prevAccPart = cloneString(psl->qName);
	    }

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

	AllocVar(coord);
	coord->start = psl->tStart;
	coord->end = psl->tEnd;
	coord->qSize = psl->qSize;
	coord->strand = sameWord(psl->strand,"+") ? 1 : 0;
	/*	when coverage is sufficient	*/
	if (percentCoverage > minCover)
	    {
	    ++partCount;
	    coord->name = cloneString(psl->qName);
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
	verbose(2,"# %s\t%u\t%u\t%u\t%.4f\t%d %s:%d-%d %s\n",
	    psl->qName, psl->qSize, tSize, tSize - psl->qSize,
	    percentCoverage, chrCount, psl->tName, psl->tStart, psl->tEnd,
	    psl->strand);
	    }
	else
	    {
	verbose(3,"# %s\t%u\t%u\t%u\t%.4f\t%d %s:%d-%d %s\n",
	    psl->qName, psl->qSize, tSize, tSize - psl->qSize,
	    percentCoverage, chrCount, psl->tName, psl->tStart, psl->tEnd,
	    psl->strand);
	    }


	freeMem(accName);
	freeMem(chrName);
	pslFree(&psl);
	}
	processResult(chrHash, coordHash, prevAccName, querySize);
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{   
optionInit(&argc, argv, options);

if (argc < 2)
    usage();

minCover = optionFloat("minCover", 50.0);

verbose(2,"#\tminCover: %% %g\n", minCover);

clusterClone(argc, argv);

return(0);
}
