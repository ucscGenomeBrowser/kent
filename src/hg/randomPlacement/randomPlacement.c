/* randomPlacement - run placement trials on a set of elements */

#include "common.h"
#include "options.h"
#include "bed.h"

/*************************************************************************

Procedure:

1. Read in bed list defining regions.  Space between the regions will be
	the gaps.  Sort to be in order by chr,chromStart
	boundingRegions
2. Read in the bed list defining the items to place in the gaps.
	Sort to be in inverse order by size, largest sized item
	first.  placedItems
3.  For loop for everything below to the number of trials specified,
	a trial count of zero should run through at least the stats
	and show the beginning set of stats.
4.  Construct new linked structure to the first bed list, call this
	the gapList (one of these for each chrom):
	next
	prev
	gapSize
	bedItem Upstream
	bedItem Downstream
	itemType Upstream (either a bounding region, or a placed item)
	itemType Downstream (either the bounding region, or a placed item)

	This should be a simple walk through of the sorted bed list
	Warn if overlapping regions, call their gap size zero.
	All these gaps will have both upstream and downstream bedItems.
	i.e. no placements before first or after last bounding items.

Loop repeats back to here

5.  Order the gapList inverse by gapSize, largest gaps first

6.  if (0 == trials) then simply place the placedItems into the gaps

7.  if (trials > 0) Walking down the placedItems list, largest ones first,

	if its size is greater than the first gap on the gapList
	we are in trouble, there is no gap large enough to fit.

	Go through the gapList counting up the number of gaps N that
	are larger or equal to the size of the item we need to place.

	That count is used to generate a random number R between 0 and N

	That random number R is the gap in which we need to add this item,
	walk down the gapList to that gap and insert this placedItem

	To insert, take (gapSize - itemSize) as a number N to generate
	a random number R 0 to N - this R added to bedItem Downstream
	chromEnd is the new chromStart for this placedItem

	Continue this procedure step 6 for all placedItems

8.  Compute statistics of the gaps with this placement.

9.  If trials > 0 repeat from step 5

10.  Display stats;  DONE;

***************************************************************************/

static struct optionSpec optionSpecs[] = {
    {"trials", OPTION_INT},
    {NULL, 0}
};

int trials = 0;

static void usage()
/* Explain usage and exit. */
{
verbose(1,
  "randomPlacement - run placement trials on a set of elements\n"
  "usage:\n"
  "   randomPlacement [options] <boundingRegions.bed> <placedItems.bed>\n"
  "options:\n"
  "   -trials=N - run N trials for final statistics (N=0 default)\n"
  "             - for N=0, the initial placement stats are displayed\n"
  "Methods:\n"
  "    The boundingRegions.bed defines elements between which are gaps.\n"
  "    The placedItems.bed defines a set of items to be placed in those gaps.\n"
  "    The usual case has the placedItems items already in the existing gaps,\n"
  "        thus their current placement statistic (nearest bounding neighbor\n"
  "        distance) can be measured with a -trials=0 (the default)\n"
);
}

struct gap
    {
    struct gap *next;	/*	double linked list	*/
    struct gap *prev;
    struct bed *upstream;	/*	bounding item upstream of this gap */
    struct bed *downstream;	/*	bounding item downstream of this gap */
    int gapSize;		/* downstream->chromStart - upstream->chromEnd*/
    boolean isUpstreamBound;	/*	TRUE == bounding item	*/
    boolean isDownstreamBound;	/*	FALSE == placed item	*/
    };

static void freeGapList(struct gap **gl)
/*	release memory for all elements on the gap List gl	*/
{
struct gap *el, *next;
for (el = *gl; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el);
    }
*gl = NULL;
}

struct chrGapList
/*	a list of gap lists, one list for each chrom	*/
    {
    struct chrGapList *next;
    char *chrom;
    struct gap *gList;
    };

static void freeChrList(struct chrGapList **cl)
{
struct chrGapList *el, *next;

for (el = *cl; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el->chrom);
    freeGapList(&el->gList);
    freeMem(el);
    }
*cl = NULL;
}

static struct bed *nextDownstreamBound(struct gap *gEl)
/*	find the next downstream bounding element	*/
{
struct gap *gl;
struct bed *downstreamBound = NULL;

for (gl = gEl; gl != NULL; gl = gl->next)
    {
    if (gl->isDownstreamBound)
	{
	downstreamBound = gl->downstream;
	break;
	}
    }
return (downstreamBound);
}

struct statistic
/*  the statistics from a measurement of gaps and placed items	*/
    {
    struct statistic *next;	/*	will accumulate a number of these */
    int chromCount;		/*	the number of chroms involved	*/
    int totalGaps;		/*	total gaps, non zero and zero	*/
    int sizeZeroGapCount;	/*	count of zero sized gaps	*/
    int maxGap;			/*	maximum gap	*/
    int minGap;			/*	minimum gap (often seen as 1)	*/
    int medianGap;		/*	median gap (of non-zero)	*/
    int meanGap;		/*	mean gap (of non-zero)		*/
    int boundingElementCount;	/* # of elements defining initial set of gaps */
    int placedItemCount;	/*	how many items were placed in gaps */
    int meanNearestNeighbor;	/*	nearest neighbor statistics	*/
    int medianNearestNeighbor;	/*	this is the nearest bounding element */
    int maximumNearestNeighbor;	/*	to the placed item	*/
    int placedWithin100bp;	/*	divide by placedItemCount for %	*/
    int zeroNeighbor;		/*	number with zero distance to neighbor */
    };

static void statsHeader()
{
printf("chrom  Bounding  Total  Size 0  Maximum  Minimum  Median  Mean\n");
printf("count  elements   gaps    gaps      gap      gap     gap   gap\n");
}

static void statsPrint(struct statistic *statList)
{
struct statistic *statEl;
statsHeader();
for (statEl = statList; statEl != NULL; statEl = statEl->next)
    {
    printf("%5d%10d%7d%8d%9d%9d%8d%6d\n",
	statEl->chromCount, statEl->boundingElementCount, statEl->totalGaps,
	statEl->sizeZeroGapCount, statEl->maxGap, statEl->minGap,
	statEl->medianGap, statEl->meanGap);
    if (statEl->placedItemCount)
	{

printf("Nearest neighbor statistics:\n");
printf("    # of Maximum  Median    Mean # within %% within # within\n");
printf("   items nearest nearest nearest   100 bp    100bp  zero bp\n");
	printf("%8d%8d%8d%8d%9d %%%7.2f%9d\n", statEl->placedItemCount,
	    statEl->maximumNearestNeighbor, statEl->medianNearestNeighbor,
	    statEl->meanNearestNeighbor, statEl->placedWithin100bp,
	    100.0 * (double)statEl->placedWithin100bp /
		(double)statEl->placedItemCount,
	    statEl->zeroNeighbor);
	}
    }
}

static struct statistic *gapStats(struct chrGapList *cList)
{
struct statistic *returnStats;
struct chrGapList *cl;
int chrCount = 0;
int gapCountNonZeroSize = 0;
int gapCountZeroSize = 0;
int totalGapSize = 0;
int maxGap = 0;
int minGap = BIGNUM;
int *gapSizeArray = NULL;
int i;
int boundingElementCount = 0;
int placedItemCount = 0;

AllocVar(returnStats);

/*	first count number of non-zero gaps	*/
for (cl=cList; cl != NULL; cl = cl->next)
    {
    struct gap *gl;
    int gapCount = 0;
    int zeroSized = 0;
    boolean firstOne = TRUE;
    for (gl = cl->gList; gl != NULL; gl = gl->next)
	{
	int gapSize = gl->gapSize;
	if (firstOne)
	    {
	    firstOne = FALSE;
	    if (gl->isUpstreamBound)
		++boundingElementCount;
	    else
		++placedItemCount;
	    }
	if (gl->isDownstreamBound)
	    ++boundingElementCount;
	else
	    ++placedItemCount;
	if (gapSize > 0)
	    {
	    totalGapSize += gapSize;
	    if (gapSize > maxGap) maxGap = gapSize;
	    if (gapSize < minGap) minGap = gapSize;
	    ++gapCount;
	    }
	else
	    {
	    ++zeroSized;
	    }
	}
    gapCountNonZeroSize += gapCount;
    gapCountZeroSize += zeroSized;
    ++chrCount;
verbose(4,"%s: %d gaps, %d of size zero\n", cl->chrom, gapCount, zeroSized);
    }
verbose(3,"counted %d chroms and %d gaps ( + %d size zero = %d total gaps)"
    "\n\ton the bounding list\n", chrCount, gapCountNonZeroSize,
	gapCountZeroSize, gapCountNonZeroSize+gapCountZeroSize);

returnStats->chromCount = chrCount;
returnStats->totalGaps = gapCountNonZeroSize + gapCountZeroSize;
returnStats->sizeZeroGapCount = gapCountZeroSize;
returnStats->boundingElementCount = boundingElementCount;

/*	now copy all the values to a integer array for more detailed
 *	stats measurements
 */
gapSizeArray = needHugeMem((size_t)(sizeof(int) * gapCountNonZeroSize));
i = 0;

for (cl=cList; cl != NULL; cl = cl->next)
    {
    struct gap *gl;
    for (gl = cl->gList; gl != NULL; gl = gl->next)
	{
	int gapSize = gl->gapSize;
	if (gapSize > 0)
	    {
	    gapSizeArray[i] = gapSize;
	    ++i;
	    }
	}
    }
verbose(3,"assigned %d values to int array\n", i);

intSort(i,gapSizeArray);

/*	0.5 rounds up to next integer	*/
returnStats->meanGap = 0.5 + ((double)totalGapSize/(double)gapCountNonZeroSize);
returnStats->maxGap = gapSizeArray[i-1];
returnStats->minGap = gapSizeArray[0];
returnStats->medianGap = gapSizeArray[i/2];

verbose(2,"average gap size: %d = %d / %d (non-zero size gaps only)\n",
    returnStats->meanGap, totalGapSize, gapCountNonZeroSize);
verbose(2,"maximum gap size: %d\n", returnStats->maxGap);
verbose(2,"median gap size: %d\n", returnStats->medianGap);
verbose(2,"minimum gap size: %d\n", returnStats->minGap);
verbose(2,"minimum gap: %d, maximum gap: %d\n", minGap, maxGap);
if (minGap != returnStats->minGap)
    errAbort("ERROR: didn't find the same minimum gap ?");
if (maxGap != returnStats->maxGap)
    errAbort("ERROR: didn't find the same maximum gap ?");
verbose(2,"bounding element count: %d, placed element count: %d\n",
	boundingElementCount, placedItemCount);


/* if there are placed elements, measure their nearest neighbor statistics */
if (placedItemCount)
    {
    int i;
    int sumDistances = 0;
    int *placedDistances =
	needHugeMem((size_t)(sizeof(int) * placedItemCount));
    int placedCount = 0;
    int boundingElements = 0;
    for (cl=cList; cl != NULL; cl = cl->next)
	{
	struct gap *gl;
	struct gap *next;
	struct bed *upstreamBound = NULL;
	struct bed *downstreamBound = NULL;

	++boundingElements; /*	first one must be bounding	*/
		/* after this, only need to count the downstream ones */

	for (gl = cl->gList; gl != NULL; gl = next)
	    {
	    struct gap *gEl;

	    /*	make note of upstream and downstream bounding elements */
	    if (NULL == upstreamBound || gl->isUpstreamBound)
		{
		if (gl->isUpstreamBound)
		    {
		    upstreamBound = gl->upstream;
		    downstreamBound = nextDownstreamBound(gl);
		    ++boundingElements;
		    if (NULL == downstreamBound)
			errAbort("Can not find a downstream"
				" bounding element ?");
		    }
		else
		    errAbort("Do not find a bounding element as"
			" first upstream item ?");
		}
	    /*	measure all downstream elements as long as they
	     *	are not bounding elements
	     */
	    for (gEl = gl; (gEl != NULL) && (! gEl->isDownstreamBound);
			gEl = gEl->next)
		{
		/* protect against negative results with the max(0,..) */
		int upstreamDist = max(0,
		    (gEl->downstream->chromStart - upstreamBound->chromEnd));
		int downstreamDist = max(0,
		    (downstreamBound->chromStart - gEl->downstream->chromEnd));
		int minDistance = min(upstreamDist, downstreamDist);

		if (minDistance < 0)
			errAbort("minimum distance < 0 ?");

		placedDistances[placedCount++] = minDistance;
		sumDistances += minDistance;
		}
	    if (gEl)
		next = gEl->next;
	    else
		next = NULL;
	    }
	}	/*	for (cl=cList; cl != NULL; cl = cl->next)	*/

    returnStats->placedItemCount = placedCount;
    returnStats->meanNearestNeighbor = 0.5 + (double)sumDistances / placedCount;

    if (boundingElements != boundingElementCount)
	errAbort("ERROR: did not find the same number of bounding elements ? %d =! %d", boundingElementCount, returnStats->boundingElementCount);


    intSort(placedCount,placedDistances);
    returnStats->medianNearestNeighbor = placedDistances[placedCount/2];
    returnStats->maximumNearestNeighbor = placedDistances[placedCount-1];

    verbose(2,"measured %d placed items\n", placedCount);
    verbose(2,"mean distance: %d = %d / %d\n",
	returnStats->meanNearestNeighbor, sumDistances, placedCount);
    verbose(2,"median distance: %d\n", returnStats->medianNearestNeighbor);
    verbose(2,"maximum distance: %d\n", returnStats->maximumNearestNeighbor);

    for (i = 0; i < placedCount; ++i)
	if (placedDistances[i] > 0) break;

    returnStats->zeroNeighbor = i + 1;

    for ( ; i < placedCount; ++i)
	if (placedDistances[i] > 100) break;


    returnStats->placedWithin100bp = (i+1) - returnStats->zeroNeighbor;

    verbose(2,"%d - number of items zero distance to nearest "
	"bounding element\n", returnStats->zeroNeighbor);
    verbose(2,"%d - number of items of non-zero distance within 100 bp of "
	"nearest bounding element\n", returnStats->placedWithin100bp);
    if ((placedCount-returnStats->zeroNeighbor) > 0)
	verbose(2,"%% %.04f - percent of of items of non-zero distance "
	    "within 100 bp of nearest bounding element\n",
		100.0 * returnStats->placedWithin100bp / (placedCount-returnStats->zeroNeighbor));
    else
       errAbort("something wrong with placed item count "
		"minus zeroDistance Count");

    }	/*	if (placedItemCount)	*/
freeMem(gapSizeArray);
return (returnStats);
}

static void placeItem(struct bed *bedEl, struct gap *gl)
/*	create two gaps where one now exists, the bedEl splits
 *	the existing gap (== gl) which becomes the new downstream gap.
 */
{
struct gap *el;
int gapSize = 0;
int itemSize = bedEl->chromEnd - bedEl->chromStart;

if (itemSize < 0)
    errAbort("ERROR: trying to insert negative sized item ?\n");

if (itemSize > gl->gapSize)
    {
    verbose(2,"item: %s:%d-%d size %d\n", bedEl->chrom, bedEl->chromStart,
	bedEl->chromEnd, itemSize);
    verbose(2,"gap:  %s:%d-%d size %d (%d)\n", gl->upstream->chrom,
	gl->upstream->chromEnd, gl->downstream->chromStart,
	gl->gapSize, gl->downstream->chromStart - gl->upstream->chromEnd );
    errAbort("ERROR: trying to insert an item larger than gap ?\n");
    }

/*	this new bedEl goes in the gap, the existing gap remains
 *	as the upstream gap, this new gap becomes downstream of this new
 *	bedEl and upstream of the downstream gap of the existing gap
 */
AllocVar(el);		/*	a new gap	*/

if (NULL == gl->downstream)
    errAbort("ERROR: trying to add element after last bounding element\n");

el->next = gl->next;	/*	could be NULL when last gap	*/
el->prev = gl;		/*	is NEVER NULL	*/
el->upstream = bedEl;	/*	this element is the new gap's upstream */
el->isUpstreamBound = FALSE;	/*	not a bounding element	*/
el->downstream = gl->downstream;	/*	can NEVER be NULL	*/
el->isDownstreamBound = gl->isDownstreamBound;
if (gl->next)		/*	could be NULL when last gap	*/
    gl->next->prev = el;
gl->downstream = bedEl;
gl->isDownstreamBound = FALSE;	/*	not a bounding element	*/
gl->next = el;

/*	new gap size, (downstream start) - (upstream end)	*/
gapSize = el->downstream->chromStart - bedEl->chromEnd;
if (gapSize >= 0)
    el->gapSize = gapSize;
else
    {
    warn("WARNING: new element insert overlaps following element\n");
    el->gapSize = 0;
    }
/*	resize existing gap, (downstream start) - (upstream end) */
gapSize = bedEl->chromStart - gl->upstream->chromEnd;
if (gapSize >= 0)
    el->prev->gapSize = gapSize;
else
    {
    warn("WARNING: new element insert overlaps previous element\n");
    el->prev->gapSize = 0;
    }
}

static void initialPlacement(struct chrGapList *bounding, struct bed *placed)
{
struct bed *bedEl;
int unplacedCount = 0;

for (bedEl = placed; bedEl != NULL; bedEl = bedEl->next)
    {
    struct chrGapList *cl;
    boolean placedOK = FALSE;
    for (cl = bounding; cl != NULL; cl = cl->next)
	{
	struct gap *gl;
        if (differentWord(cl->chrom, bedEl->chrom))
		continue;
	for (gl = cl->gList; gl != NULL; gl = gl->next)
	    {
	    if ( ((gl->downstream != NULL) && (gl->upstream != NULL)) &&
		    ((bedEl->chromEnd <= gl->downstream->chromStart) &&
			(bedEl->chromStart >= gl->upstream->chromEnd)) )
		{
		placeItem(bedEl, gl);
		placedOK = TRUE;
		verbose(5,"item: %s:%d-%d, gap: %s:%d-%d\n",
		    bedEl->chrom, bedEl->chromStart, bedEl->chromEnd,
			bedEl->chrom, gl->upstream->chromEnd,
			    gl->downstream->chromStart);
		break;
		}
	    }
	}
    if (! placedOK)
	{
	verbose(4,"can not place item: %s:%d-%d\n", bedEl->chrom,
		bedEl->chromStart, bedEl->chromEnd);
	++unplacedCount;
	}
    }
if (unplacedCount)
    verbose(2,"Could not place %d items\n", unplacedCount);
}

static struct chrGapList *createGaps(struct bed *bounds)
{
struct bed *bedEl = NULL;
char *prevChr = NULL;
struct chrGapList *gaps = NULL;
struct gap *prevGap = NULL;
struct bed *prevBedEl = NULL;
struct chrGapList *curChrList = NULL;
int boundingChrCount = 0;
int overlappedBounding = 0;

for (bedEl = bounds; bedEl != NULL; bedEl = bedEl->next)
    {
    /*	the first bedEl does not yet start a new gap, must have a second */
    if ((NULL == prevChr) || differentWord(prevChr,bedEl->chrom))
	{
	struct chrGapList *cEl;
	AllocVar(cEl);
	cEl->chrom = cloneString(bedEl->chrom);
	cEl->gList = NULL;
	prevGap = NULL;
	prevBedEl = bedEl;	/*	bounding element before first gap */
	if (prevChr)
	    freeMem(prevChr);
	prevChr = cloneString(bedEl->chrom);
	verbose(4,"new chrom on bounding gap creation %s\n", prevChr);
	slAddHead(&gaps,cEl);
	++boundingChrCount;
	curChrList = cEl;
	}
    else
	{
	struct gap *gEl;
	AllocVar(gEl);
	gEl->prev = prevGap;	/*	first one is NULL	*/
	gEl->upstream = prevBedEl;
	gEl->isUpstreamBound = TRUE;	/*	bounding element	*/
	gEl->downstream = bedEl;
	gEl->isDownstreamBound = TRUE;	/*	bounding element */
	gEl->next = NULL;		/*	not there yet	*/

	if (prevGap == NULL)	/*	first one is NULL	*/
	    {
	    curChrList->gList = gEl;	/*	starting the list	*/
	    }
	else
	    {
	    prevGap->next = gEl;
	    }

	prevGap = gEl;

	/*	gapSize is between downstream and upstream	*/
	gEl->gapSize = bedEl->chromStart - prevBedEl->chromEnd;
	verbose(5,"gap: %s:%d-%d size %d (%d)\n",
		bedEl->chrom, gEl->upstream->chromEnd,
			gEl->downstream->chromStart, gEl->gapSize,
			gEl->downstream->chromStart - gEl->upstream->chromEnd);
	if (gEl->gapSize < 0)
	    {
	    ++overlappedBounding;
	    if (verboseLevel()>3)
		{
		warn("WARNING: overlapping bounding elements at\n\t"
		    "%s:%d-%d <-> %s:%d-%d",
			prevBedEl->chrom, prevBedEl->chromStart,
			    prevBedEl->chromEnd, bedEl->chrom,
				bedEl->chromStart, bedEl->chromEnd);
		}
	    gEl->gapSize = 0;
	    }
	prevBedEl = bedEl;
	}
    }

if (prevChr) freeMem(prevChr);
slReverse(&gaps);
verbose(3,"bounding chrom count: %d (=? %d), overlapped items: %d\n",
	boundingChrCount, slCount(gaps), overlappedBounding);

return(gaps);
}

static struct chrGapList *cloneGapList(struct chrGapList *gaps)
/*	make an independent copy of the gaps list, they share the bed
 *	elements, but that is all.	*/
{
struct chrGapList *el;
struct chrGapList *cloneGaps = NULL;
int chrCount = 0;
int totalGapCount = 0;

for (el = gaps; el != NULL; el = el->next)
    {
    struct gap *gap;
    struct chrGapList *gl;
    struct gap *prevGap = NULL;
    boolean firstGap = TRUE;
    int nonZeroGapCount = 0;
    int zeroGapCount = 0;

    ++chrCount;
    AllocVar(gl);
    gl->chrom = cloneString(el->chrom);

    for (gap = el->gList; gap != NULL; gap = gap->next)
	{
	struct gap *g;
	AllocVar(g);
	g->next = NULL;
	g->prev = prevGap;
	if (prevGap != NULL)
	    prevGap->next = g;
	g->upstream = gap->upstream;
	g->downstream = gap->downstream;
	g->gapSize = gap->gapSize;
	g->isUpstreamBound = gap->isUpstreamBound;
	g->isDownstreamBound = gap->isDownstreamBound;
	prevGap = g;
	if (firstGap)
	    {
	    gl->gList = g;
	    firstGap = FALSE;
	    }
	if (gap->gapSize > 0)
	    ++nonZeroGapCount;
	else
	    ++zeroGapCount;
	}
    totalGapCount += nonZeroGapCount;
    slAddHead(&cloneGaps,gl);
    }
slReverse(&cloneGaps);
return(cloneGaps);
}

static void randomPlacement(char *bounding, char *placed)
{
struct bed *boundingElements = bedLoadAll(bounding);
struct bed *placedItems = bedLoadAll(placed);
int boundingCount = slCount(boundingElements);
int placedCount = slCount(placedItems);
struct chrGapList *boundingGaps = NULL;
struct chrGapList *duplicateGapList = NULL;
struct statistic *statsList = NULL;
struct statistic *statEl = NULL;

slSort(&boundingElements, bedCmp);	/* order by chrom,chromStart */
slSort(&placedItems, bedCmp);		/* order by chrom,chromStart */

verbose(2, "bounding element count: %d\n", boundingCount);
verbose(2, "placed item count: %d\n", placedCount);

boundingGaps = createGaps(boundingElements);

if (0 == trials)
    {
    duplicateGapList = cloneGapList(boundingGaps);

    verbose(2,"stats before initial placement:  =================\n");
    statEl = gapStats(duplicateGapList);
    printf("statistics on gaps before any placements:\n\t(%s)\n", bounding);
    statsPrint(statEl);
    slAddHead(&statsList,statEl);

    initialPlacement(duplicateGapList,placedItems);

    verbose(2,"stats after initial placement:  =================\n");
    statEl = gapStats(duplicateGapList);
    printf("statistics after initial placement of placed items:\n\t(%s)\n",
		placed);
    statsPrint(statEl);
    slAddHead(&statsList,statEl);

    freeChrList(&duplicateGapList);
    slReverse(&statsList);
    }
else
    {
    int trial;
    for (trial = 0; trial < trials; ++trial)
	{
	duplicateGapList = cloneGapList(boundingGaps);
	statEl = gapStats(duplicateGapList);
	slAddHead(&statsList,statEl);
	freeChrList(&duplicateGapList);
	}
    }
bedFreeList(&boundingElements);
bedFreeList(&placedItems);
freeChrList(&boundingGaps);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
trials = optionInt("trials", 0);

verbose(2,"bounding elements file: %s\n", argv[1]);
verbose(2,"placed items file: %s\n", argv[2]);
verbose(2,"number of trials: %d\n", trials);

randomPlacement(argv[1], argv[2]);
verbose(3,"at exit of main()\n");
return(0);
}
