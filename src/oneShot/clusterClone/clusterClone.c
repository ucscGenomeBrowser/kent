/*	clusterClone - cluster together clone psl alignment results */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "psl.h"
#include "obscure.h"

static char const rcsid[] = "$Id: clusterClone.c,v 1.6 2004/06/18 23:54:38 hiram Exp $";

float minCover = 80.0;		/*	percent coverage to count hit */
unsigned maxGap = 1000;		/*	maximum gap between pieces */
boolean agp = FALSE;		/*	AGP output requested, default BED */

static struct optionSpec options[] = {
   {"minCover", OPTION_FLOAT},
   {"maxGap", OPTION_INT},
   {"agp", OPTION_BOOLEAN},
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
  "   -minCover=P - minimum P percent coverage to count an alignment (default 80.0)\n"
  "   -maxGap=M - maximum M bases gap between pieces to stitch together, default 1000\n"
  "   -agp - output AGP format (default is bed format)\n"
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

static int startCompare(const void *va, const void *vb)
/*	Compare to sort on start coordinates	*/
{
const struct coordEl *a = *((struct coordEl **)va);
const struct coordEl *b = *((struct coordEl **)vb);
return (int)a->start - (int)b->start;
}

static int endDescending(const void *va, const void *vb)
/*	Compare to inverse sort on end coordinates	*/
{
const struct coordEl *a = *((struct coordEl **)va);
const struct coordEl *b = *((struct coordEl **)vb);
return (int)b->end - (int)a->end;
}

/*	Start at the calculated median point, scan through the
 *	coordinates and adjust the start and end of the clustered region
 *	to include the appropriate section.
 */
static void extendLimits(struct coordEl **coordListPt, unsigned median,
    unsigned querySize, unsigned *startExtended, unsigned *endExtended,
    char *ctgName)
{
struct coordEl *coord;
unsigned halfLength = querySize / 2;
boolean firstCoordinate = TRUE;

if (halfLength > median)
    *startExtended = 0;
else
    *startExtended = median - halfLength;

*endExtended = median + halfLength;
verbose(2,"# starting limits: %u - %u\n", *startExtended, *endExtended);

/*	sort the list descending by end coordinates	*/
slSort(coordListPt,endDescending);

if (coordListPt) coord = *coordListPt;
else coord = NULL;

/*	Walk through this list extending the start.
 *	Same discussion as below, although reverse the sense of start
 *	and end.  Here the list is sorted in descending order by end
 *	coordinate.  Those end coordinates are compared with the
 *	extending start coordinate to move it out.
 */
verbose(2,"# after end sort\n");

firstCoordinate = TRUE;
while (coord != NULL)
    {
    if (firstCoordinate)
	{
	if (*endExtended > coord->end)
	    {
	    *endExtended = coord->end;
	    verbose(2,"# end brought in to: %u\n", *endExtended);
	    }
	firstCoordinate = FALSE;
	}
    verbose(2,"# %s %u - %u %u %u %c\n", coord->name, coord->start, coord->end, *startExtended, coord->qSize, (coord->strand == 1) ? '+' : '-');
    if (coord->end < *startExtended)
	{
	unsigned gap = *startExtended - coord->end;

	if (gap > maxGap)
	    {
	    verbose(2,"# more than max Gap encountered: %u\n", gap);
	    break;	/*	exit this while loop	*/
	    }
	*startExtended = coord->start;
	verbose(2,"# start extended to: %u\n", *startExtended);
	}
    else if (coord->start < *startExtended)
	{
	*startExtended = coord->start;
	verbose(2,"# start extended to: %u\n", *startExtended);
	}

    coord = coord->next;
    }

/*	sort the list by start coordinates	*/
slSort(coordListPt,startCompare);

if (coordListPt) coord = *coordListPt;
else coord = NULL;

/*	Walk through this list extending the end.  The list is in order
 *	by start coordinates.  Going down that list checking the
 *	extended end with these start coordinates, eventually we reach a
 *	point where the start coordinates are past the end leaving a
 *	gap.  As long as the gap is within the specified maxGap limit,
 *	then it is OK to jump to that next piece.  The new end becomes
 *	the end of this new piece.
 *	And secondly, even if the starts aren't past the extending end,
 *	the piece under examination may have a new end that is longer,
 *	in which case the extending end moves to that point.
 */
verbose(2,"# extending end\n");

/*	The first coordinate check will ensure that the extended start
 *	coordinate is not less than the smallest start coordinate.
 *	Thus, only the actual part coverage will determine the maximum
 *	limits and we won't go beyond the parts.
 */
firstCoordinate = TRUE;
while (coord != NULL)
    {
    if (firstCoordinate)
	{
	if (*startExtended < coord->start)
	    {
	    *startExtended = coord->start;
	    verbose(2,"# start brought in to: %u\n", *startExtended);
	    }
	firstCoordinate = FALSE;
	}
    verbose(2,"# %s %u %u - %u %u %c\n", coord->name, *endExtended, coord->start, coord->end, coord->qSize, (coord->strand == 1) ? '+' : '-');
    if (coord->start > *endExtended)
	{
	unsigned gap = coord->start - *endExtended;

	if (gap > maxGap)
	    {
	    verbose(2,"# more than max Gap encountered: %u\n", gap);
	    break;	/*	exit this while loop	*/
	    }
	*endExtended = coord->end;
	verbose(2,"# end extended to: %u\n", *endExtended);
	}
    else if (coord->end > *endExtended)
	{
	*endExtended = coord->end;
	verbose(2,"# end extended to: %u\n", *endExtended);
	}
    coord = coord->next;
    }

/*	If agp output, we need to do it here	*/
if (agp)
    {
    if (coordListPt) coord = *coordListPt;
    else coord = NULL;

    int cloneCount = 0;
    while (coord != NULL)
	{
	if ( (coord->start >= *startExtended) &&
		(coord->end <= *endExtended))
	    {
	    ++cloneCount;
    printf("%s\t%u\t%u\t%d\tD\t%s\t%u\t%u\t%c\n", ctgName, coord->start,
	coord->end, cloneCount, coord->name,
	coord->start - *startExtended + 1, coord->end - *startExtended + 1,
	(coord->strand == 1) ? '+' : '-');
	    }
	coord = coord->next;
	}
    }
}	/*	static void extendLimits() */

static void processResult(struct hash *chrHash, struct hash *coordHash,
	char *accName, unsigned querySize)
{
struct hashCookie cookie;
struct hashEl *hashEl;
int highCount = 0;
int secondHighest = 0;
char *ctgName = (char *)NULL;
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
verbose(2,"# %s %d\n", hashEl->name, count);
    if (count >= highCount)
	{
	secondHighest = highCount;
	if (secondHighestName)
	    freeMem(secondHighestName);
	highCount = count;
	if (ctgName)
	    {
	    secondHighestName = cloneString(ctgName);
	    freeMem(ctgName);
	    }
	ctgName = cloneString(hashEl->name);
	}
    }
verbose(2,"# %s %d highest count, next: %s %d\n", ctgName, highCount, secondHighestName, secondHighest);

if (highCount == secondHighest)
    {
verbose(1,"# ERROR TIE for high count %s %d highest count, next: %s %d  TIE *\n", ctgName, highCount, secondHighestName, secondHighest);
    }

/*	for that highest count chrom, examine its coordinates, find high
 *	and low */
coords = hashLookup(coordHash, ctgName);

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
    unsigned startExtended;
    unsigned endExtended;

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
    extendLimits(coordListPt, median, querySize, &startExtended, &endExtended,
	ctgName);
    verbose(2,
	"# qSize: %u, Median: %u implies %u-%u %s\n#\textended to%u-%u\n",
	querySize, median, median - (querySize/2), median+(querySize/2),
	accName, startExtended, endExtended);
    /*	if BED output, output the line here, AGP was done in extendLimits */
    if (!agp)
	{
    printf("%s\t%u\t%u\t%s\t%c\n", ctgName, startExtended, endExtended,
	accName, strandSum > (coordCount/2) ? '+' : '-');
	}
    freeMem(midPoints);
    }
else
    {
    verbose(1,"# ERROR - no coordinates found ? %s\n", ctgName);
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
freeMem(ctgName);
}	/*	static void processResult()	*/

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
		verbose(1,"# ERROR - no coordinates found ? %s\n", prevAccName);
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
	/*stripString(chrName, "chr");*/
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
}	/*	static void clusterClone()	*/

int main(int argc, char *argv[])
/* Process command line. */
{   
optionInit(&argc, argv, options);

if (argc < 2)
    usage();

minCover = optionFloat("minCover", 80.0);
maxGap = optionInt("maxGap", 1000);
agp = optionExists("agp");

verbose(2,"#\tminCover: %% %g\n", minCover);
verbose(2,"#\tmaxGap: %u\n", maxGap);
verbose(2,"#\toutput type: %s\n", agp ? "AGP" : "BED");

clusterClone(argc, argv);

return(0);
}
