/* randomPlacement - run placement trials on a set of elements */

#include "common.h"
#include "options.h"
#include "bed.h"

/*************************************************************************

Procedure:

1. Read in bed list defining bounding regions.  Space between the bounding
	regions will be the gaps in which items can be placed.
	Sort to be in order by chr,chromStart.  Construct a master
	list of gaps defined by these bounding elements to be copied
	and used repeatedly later for placement trials (fixed or random).
2. Read in the bed list defining the items to place in the gaps.
	Initially sort to be in order by chr,chromStart so they
	can easily be placed into the bounding region gaps just to see
	what the initial placement looks like.  Later, during the trial
	runs, this list of elements will be sorted inverse order by size
	so the largest items can be randomly placed first, and the list
	worked through by size.
3. Optionally, read in a bed list of items that are to be the measured
	standard for nearest neighbor stats.  If there isn't one of these
	supplied, than the original defining bounding regions are the elements
	to measure nearest neighbor statistics.  This list too creates a
	master gap list to be copied and used repeatedly during nearest
	neighbor measurements.
4. Show initial statistics on amount of gaps available between bounding
	regions, and the length statistics of the items to be placed.
	If zero trials are requested, simply place the items in the
	gaps at their specified positions, and measure their nearest
	neighbor statistics.
5.  for() loop for everything below to the number of trials specified,
6.  Make a copy of the master gap list for placement.

7.  For each item to be placed, largest ones first
    a.  construct an array of pointers to gaps that are at least as
	large as the item to be placed.
    b.  Randomly select from that array list, one of the gaps in which
	to place the item.
    c.  In the gap, select a random start location for the item to
	be placed with enough room for the item to fit from the selected
	start location to the length of the item.
    d.  Repeat this placement procedure for all the items to be placed

8.  Compute nearest neighbor statistics.  If we have been supplied
	with an optional list to do the measurement to, copy the items
	from the random placements into a copy of the gap list created
	from the optional measurement items.
9.  Display nearesot neighbor statistics.
10.  Release the memory used by these temporary gap lists for the placements.
11.  Repeat trial loop back to step 6.
	
***************************************************************************/

static struct optionSpec optionSpecs[] = {
    {"trials", OPTION_INT},
    {"seed", OPTION_INT},
    {"shoulder", OPTION_INT},
    {"neighbor", OPTION_STRING},
    {"bed", OPTION_STRING},
    {"zeroBed", OPTION_STRING},
    {NULL, 0}
};

int verbosity = 1;
int trials = 0;
int seed = 0;
int shoulder = 100;
char *neighbor = NULL;
char *bedOutFile = NULL;
char *zeroBedOutFile = NULL;

static void usage()
/* Explain usage and exit. */
{
verbose(1,
  "randomPlacement - run placement trials on a set of elements\n"
  "usage:\n"
  "   randomPlacement [options] <boundingRegions.bed> <placedItems.bed>\n"
  "       where the gaps between the boundingRegions items are the gaps in\n"
  "       which the placedItems are to be randomly placed for nearest\n"
  "       neighbor measurement to the boundingRegion items.  The usual case\n"
  "       has the placedItems as actual, already situated, items in the gaps\n"
  "       between the boundingRegions items.  Thus, the random placement\n"
  "       performed here is the equivalent of picking up all the placedItems\n"
  "       to clear the gaps completely, then dropping them back into the gaps\n"
  "       to see if their random placement is significantly different than\n"
  "       their already existing placement.\n"
  "options:\n"
  "   -trials=N - run N trials for final statistics (N=0 default)\n"
  "             - for N=0, the initial placement stats are displayed\n"
  "   -seed=S - seed value for drand48() - default is zero\n"
  "           - which produces the same set of random numbers each time.\n"
  "   -shoulder=D - distance D from bounding elements to measure percent\n"
  "           - of items within that distance to the bounding element,\n"
  "           - default is 100 bases\n"
  "   -neighbor=<bed file> - measure nearest neighbor statistics to these\n"
  "           - bed items instead of the boundingRegions items.  For\n"
  "           - this case, the boundingRegions gaps should be a subset of\n"
  "           - these neighbor gaps because otherwise items would be placed\n"
  "           - in locations on top of these neighbor items.  Placed items\n"
  "           - that do overlap the neighbor or boundingRegions are discarded\n"
  "           - for the nearest neighbor measurements.\n"
  "   -bed=<file name> - output bed file for the final trial placement\n"
  "   -zeroBed=<file name> - output bed file for the items that\n"
  "                        - are within 0bp of the measured neighbors"
);
exit(255);
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

static void freeGapList(struct gap **gl, boolean freeBedEl)
/*	release memory for all elements on the gap List gl
 *	If freeBedEl is TRUE, release the bed element memory too
 *	for *only* those bed elements that are *NOT* bounding elements.
 *	This is because during the random trials, temporary bed elements
 *	were created for the insertions, thus they need to be released.
 *	They only need their memory released, nothing else, for example
 *	the chrom name was not cloned.
 */
{
struct gap *el, *next;
for (el = *gl; el != NULL; el = next)
    {
    next = el->next;
    if ( freeBedEl && (el->downstream != NULL) && (! el->isDownstreamBound) )
	freeMem(el->downstream);
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

static void freeChrList(struct chrGapList **cl, boolean freeBedEl)
/*	free up all space on the given gap list cl, if freeBedEl is
 *	TRUE, then also free up the space of the "inserted" bed elements
 *	that are on the gap list.
 */
{
struct chrGapList *el, *next;

for (el = *cl; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el->chrom);
    freeGapList(&el->gList, freeBedEl);
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
    int placedWithinShoulder;	/*	divide by placedItemCount for %	*/
    int zeroNeighbor;		/*	number with zero distance to neighbor */
    };

static void statsHeader()
{
printf("chrom  Bounding   Total  Size 0   Maximum  Minimum  Median  Mean\n");
printf("count  elements    gaps    gaps       gap      gap     gap   gap\n");
}

static void statsPrint(struct statistic *statList)
{
struct statistic *statEl;
boolean firstHeader = TRUE;
double fixedMeasure = 1.0;

statsHeader();
for (statEl = statList; statEl != NULL; statEl = statEl->next)
    {
    if (firstHeader)
	{
	printf("%5d%10d%8d%8d%10d%9d%8d%6d\n",
	    statEl->chromCount, statEl->boundingElementCount,
	    statEl->totalGaps, statEl->sizeZeroGapCount, statEl->maxGap,
	    statEl->minGap, statEl->medianGap, statEl->meanGap);
	}
    if (statEl->placedItemCount)
	{
	double shoulderPercent = 100.0 * (double)statEl->placedWithinShoulder /
		(double)statEl->placedItemCount;

	if (firstHeader)
	    {
	    printf("Nearest neighbor statistics:\n");
	    printf("    # of   Maximum  Median    Mean # within "
		"%% within # within    ratio   shoulder\n");
	    printf("   items   nearest nearest nearest   %d bp    "
		"%dbp  zero bp  fixed/random  window\n", shoulder, shoulder);
	    fixedMeasure =  shoulderPercent;
	    }
	printf("%8d%10d%8d%8d%9d %%%7.2f%9d%7.2f%8d\n", statEl->placedItemCount,
	    statEl->maximumNearestNeighbor, statEl->medianNearestNeighbor,
	    statEl->meanNearestNeighbor, statEl->placedWithinShoulder,
	    shoulderPercent,
	    statEl->zeroNeighbor, fixedMeasure/shoulderPercent, shoulder);
	}
    firstHeader = FALSE;
    }
}

static struct statistic *gapStats(struct chrGapList *cList, char *zeroBedFile)
{
struct statistic *returnStats;
struct chrGapList *cl;
int chrCount = 0;
int gapCountNonZeroSize = 0;
int gapCountZeroSize = 0;
unsigned long totalGapSize = 0;
int maxGap = 0;
int minGap = BIGNUM;
int *gapSizeArray = NULL;
int i;
int boundingElementCount = 0;
int placedItemCount = 0;
FILE *zeroFH = NULL;

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
verbose(4,"'%s': %d gaps + %d of size zero = %d\n", cl->chrom, gapCount,
	zeroSized, gapCount+zeroSized);
    }

if (((char *)NULL != zeroBedFile) && (gapCountZeroSize > 0))
    zeroFH=mustOpen(zeroBedFile, "w");

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
if (0 == gapCountNonZeroSize)
    {
    returnStats->meanGap = 0.0;
    returnStats->maxGap = maxGap;
    returnStats->minGap = minGap;
    returnStats->medianGap = 0;
    }
else
    {
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
    returnStats->meanGap = 0.5 +
	((double)totalGapSize/(double)gapCountNonZeroSize);
    returnStats->maxGap = gapSizeArray[i-1];
    returnStats->minGap = gapSizeArray[0];
    returnStats->medianGap = gapSizeArray[i/2];
    }

verbose(2,"average gap size: %d = %ul / %d (non-zero size gaps only)\n",
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
	int chrCount = 0;

	++boundingElements; /*	first one must be bounding	*/
		/* after this, only need to count the downstream ones */

	for (gl = cl->gList; gl != NULL; gl = next)
	    {
	    struct gap *gEl;
	    int gapCount = 0;

	    ++chrCount;

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
		if (zeroFH && (minDistance == 0))
		    {
		    ++gapCount;
		    fprintf (zeroFH, "%s\t%d\t%d\t%s_%d.%d\n",
			cl->chrom, gEl->downstream->chromStart,
			gEl->downstream->chromEnd, cl->chrom,
			chrCount, gapCount);
		    }

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
	errAbort("ERROR: did not find the same number of bounding elements ? %d ? %d =! %d", boundingElements, boundingElementCount, returnStats->boundingElementCount);


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
	if (placedDistances[i] > shoulder) break;

    /*	for a while we were counting this without the zero distances */
    returnStats->placedWithinShoulder = (i+1) - returnStats->zeroNeighbor;
    returnStats->placedWithinShoulder = i;

    verbose(2,"%d - number of items zero distance to nearest "
	"bounding element\n", returnStats->zeroNeighbor);
    verbose(2,"%d - number of items of non-zero distance within %d bp of "
	"nearest bounding element\n", returnStats->placedWithinShoulder,
		shoulder);
    if ((placedCount - returnStats->zeroNeighbor) > 0)
	verbose(2,"%% %.04f - percent of of items of non-zero distance "
	    "within %d bp of nearest bounding element\n",
		100.0 * returnStats->placedWithinShoulder /
			(placedCount-returnStats->zeroNeighbor),
		shoulder);
    else
       errAbort("something wrong with placed item count %d "
	    "minus zeroDistance Count %d",
		placedCount, returnStats->zeroNeighbor);

    freeMem(placedDistances);
    }	/*	if (placedItemCount)	*/
carefulClose(&zeroFH);
freeMem(gapSizeArray);
return (returnStats);
}

static void gapInsert(struct bed *bedEl, struct gap *gl)
/*	insert the bedEl into the gap list item gl, returns pointer to
 *	a chrom name of the surrounding bounding elements	*/
{
struct gap *newGap;
int gapSize = 0;

if (NULL == gl->downstream)
    errAbort("ERROR: trying to add element after last bounding element\n");

/*	this new bedEl goes in the gap, the existing gap remains
 *	as the upstream gap, this new gap becomes downstream of this new
 *	bedEl and upstream of the downstream gap of the existing gap
 */
AllocVar(newGap);		/*	a new gap	*/
newGap->next = gl->next;	/*	could be NULL when last gap	*/
newGap->prev = gl;		/*	is NEVER NULL	*/
newGap->upstream = bedEl;	/*	this element is the new gap's upstream */
newGap->isUpstreamBound = FALSE;	/*	not a bounding element	*/
newGap->downstream = gl->downstream;	/*	can NEVER be NULL	*/
newGap->isDownstreamBound = gl->isDownstreamBound;
if (gl->next)		/*	could be NULL when last gap	*/
    gl->next->prev = newGap;
gl->downstream = bedEl;
gl->isDownstreamBound = FALSE;	/*	not a bounding element	*/
gl->next = newGap;

/*	new gap size, (downstream start) - (upstream end)	*/
gapSize = newGap->downstream->chromStart - bedEl->chromEnd;
if (gapSize >= 0)
    newGap->gapSize = gapSize;
else
    {
    warn("WARNING: new element insert overlaps following element\n");
    newGap->gapSize = 0;
    }
/*	resize existing gap, (downstream start) - (upstream end) */
gapSize = bedEl->chromStart - gl->upstream->chromEnd;
if (gapSize >= 0)
    newGap->prev->gapSize = gapSize;
else
    {
    warn("WARNING: new element insert overlaps previous element\n");
    newGap->prev->gapSize = 0;
    }
}	/*	static void gapInsert(struct bed *bedEl, struct gap *gl) */

static void placeItem(struct bed *bedEl, struct gap *gl)
/*	create two gaps where one now exists, the bedEl splits
 *	the existing gap (== gl) which becomes the new downstream gap.
 */
{
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

gapInsert(bedEl, gl);
}

static int gapsOfSize(struct chrGapList *bounding, int size,
	struct gap *array[], int arraySize)
/*	from bounding gap list, find gaps greater than specified size,
 *		and add pointers to those gaps into the array, no more
 *		than the given arraySize
 */
{
struct chrGapList *cl;
int gapCount = 0;

for (cl = bounding; (cl != NULL) && (gapCount < arraySize); cl = cl->next)
    {
    struct gap *gEl;
    for (gEl = cl->gList; (gEl != NULL) && (gapCount < arraySize);
		gEl = gEl->next)
	{
	if (gEl->gapSize >= size)
	    {
	    array[gapCount++] = gEl;
	    }
	}
    }
if (gapCount < 1)
    errAbort("ERROR: could not find any gaps of size >= %d ?\n", size);
if (gapCount == arraySize)
    errAbort("ERROR: never expected to over fill the gap array ?");
return(gapCount);
}

static int countGaps(struct chrGapList *bounding)
{
int gapCount = 0;
struct chrGapList *cl;

for (cl = bounding; cl != NULL; cl = cl->next)
    {
    struct gap *gEl;
    for (gEl = cl->gList; gEl != NULL; gEl = gEl->next)
	{
	++gapCount;
	}
    }
return(gapCount);
}

static void measurePlaced(struct bed *placed)
{
struct bed *bedEl;
int placedCount = slCount(placed);
int *sizeArray = NULL;
int i = 0;

sizeArray = needHugeMem((size_t)(sizeof(int) * placedCount));
i = 0;
for (bedEl = placed; bedEl != NULL; bedEl = bedEl->next)
    {
    sizeArray[i++] = bedEl->chromEnd - bedEl->chromStart;
    }
verbose(2,"placed item size range: [%d : %d]\n", sizeArray[0], sizeArray[i-1]);
verbose(2,"placed item median: %d\n", sizeArray[i/2]);
intSort(i,sizeArray);
verbose(2,"placed item size range: [%d : %d]\n", sizeArray[0], sizeArray[i-1]);
verbose(2,"placed item median: %d\n", sizeArray[i/2]);
freeMem(sizeArray);
}

static struct bed *randomInsert(struct bed *bedEl, struct gap *gl)
/*	insert the bedEl into the gap at a random location	*/
/*	PLEASE NOTE: these specially inserted items are allocated here,
 *	they need to be freed when the gap list they are in is freed.
 */
{
struct bed *newBed;
int itemSize = bedEl->chromEnd - bedEl->chromStart;
int spareSpace = 0;		/*	amount of gap left after insert */
int placementOffset = 0;	/*	from beginning of gap	*/

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
 *	bedEl and upstream of the downstream gap of the existing gap.
 *	The spacing of this new insert is a random amount of space.
 */
AllocVar(newBed);		/*	and a new bed element	*/
		/* the bed element is *new* because it has different
		 * coordinates than the placed bed element
		 */

spareSpace = gl->gapSize - itemSize;
/*	placementOffset is the interval: [0,spareSpace) == [0,spareSpace-1] */
if (spareSpace > 0)
    placementOffset = floor(spareSpace * drand48());
else
    placementOffset = 0;

/*  XXX the chrom name of this thing needs to be the same chrom name of
 *  the surrounding bounding elements, not the incoming bedEl
 */
newBed->chromStart = gl->upstream->chromEnd + placementOffset;
newBed->chromEnd = newBed->chromStart + itemSize;
newBed->chrom = gl->upstream->chrom;	/* no need to clone, not freed	*/

gapInsert(newBed, gl);
return(newBed);
}

static struct bed *randomTrial(struct chrGapList *bounding, struct bed *placed)
/*	placed bed list has already been sorted by size descending,
	return is the newly placed bed list	*/
{
struct bed *bedList = NULL;
struct bed *bedEl;
int placedCount = slCount(placed);
int gapCount = countGaps(bounding);
int i;
struct gap **sizedGaps = NULL;	/*	an array of pointers	*/
int maxGapCount = 0;

/*	We should never have more gaps than the initial set of gaps plus
 *	the placed item count since each placed item only creates one
 *	new gap.  This array will be used repeatedly as lists of gaps of
 *	specific sizes are created.  The array will be an array of
 *	pointers to the gaps greater than the specified size.
 *	The + 1 on the maxGapCount is to keep the array one larger than
 *	expected maximum so that a safety check can be performed that it
 *	never reaches past the expected maximum.
 */
maxGapCount = placedCount + gapCount + 1;
sizedGaps = needHugeMem((size_t)(sizeof(struct gap *) * maxGapCount));
i = 0;

for (bedEl = placed; bedEl != NULL; bedEl = bedEl->next)
    {
    struct bed *newBed;
    int N;
    int R;
    int itemSize = bedEl->chromEnd - bedEl->chromStart;
    if (itemSize < 1)
	errAbort("ERROR: placing items less than 1 bp in length ? %s:%d-%d",
	bedEl->chrom, bedEl->chromEnd, bedEl->chromStart);
    N = gapsOfSize(bounding,itemSize, sizedGaps, maxGapCount);
    /*	From those N gaps, randomly select one of them	(drand48 = [0.0,1.0)*/
    R = floor(N * drand48());	/*	interval: [0,N) == [0,N-1]	*/
    if ((R >= N) || (R >= maxGapCount))
	errAbort("ERROR: did not expect random "
	    "number %d to be >= %d (or %d)\n", R, N, maxGapCount);
    /*	The newBed is the bedEl translated to a new random location */
    newBed = randomInsert(bedEl,sizedGaps[R]);
    slAddHead(&bedList,newBed);
    }
/*	sizedGaps are just a bunch of pointers, the bed element inserts
 *	actually went into the bounding gap list which is going to be
 *	freed up, along with the specially added bed elements back in
 *	the loop that is managing the copying of the bounding list.
 */
freeMem(sizedGaps);
return(bedList);
}

static void initialPlacement(struct chrGapList *bounding, struct bed *placed)
/*	simply place the items from the 'placed' list into the gaps
 *	defined by the 'bounding' list, the locations of the 'placed'
 *	items are what they say they are in their bed structure.
 */
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
	verbose(3,"can not place item: %s:%d-%d\n", bedEl->chrom,
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
	if (prevChr)
	    {
	    if (NULL == prevGap)
		{
		verbose(2,"WARNING: only one element on %s ! No gap defined.\n",
			prevChr);
 		slPopHead(&gaps);
		--boundingChrCount;
		}
	    freeMem(prevChr);
	    }
	prevChr = cloneString(bedEl->chrom);
	prevGap = NULL;
	prevBedEl = bedEl;	/*	bounding element before first gap */
	verbose(4,"new chrom on bounding gap creation %s, adding %#x\n",
		prevChr, (unsigned) cEl);
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

if (prevChr)
    {
    /*	potentially the last one is a single item on a chrom	*/
    if (NULL == prevGap)
	{
	verbose(2,"WARNING: only one element on %s ! No gap defined.\n",
		prevChr);
	slPopHead(&gaps);
	--boundingChrCount;
	}
    freeMem(prevChr);
    }

slReverse(&gaps);
verbose(3,"bounding chrom count: %d (=? %d), overlapped items: %d\n",
	boundingChrCount, slCount(gaps), overlappedBounding);

return(gaps);
}

static struct chrGapList *cloneGapList(struct chrGapList *gaps)
/*	make an independent copy of the gaps list, they share the bed
 *	elements, but that is all.	*/
{
struct chrGapList *cl;
struct chrGapList *cloneGaps = NULL;
int chrCount = 0;
int totalGapCount = 0;

for (cl = gaps; cl != NULL; cl = cl->next)
    {
    struct gap *gap;
    struct chrGapList *gl;
    struct gap *prevGap = NULL;
    boolean firstGap = TRUE;
    int nonZeroGapCount = 0;
    int zeroGapCount = 0;

    ++chrCount;
    AllocVar(gl);
    gl->chrom = cloneString(cl->chrom);

    for (gap = cl->gList; gap != NULL; gap = gap->next)
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
verbose(4,"cloneGaps: '%s': %d non-zero gaps + %d zero gaps = %d total gaps\n",
	cl->chrom, nonZeroGapCount, zeroGapCount, nonZeroGapCount+zeroGapCount);
    slAddHead(&cloneGaps,gl);
    }
slReverse(&cloneGaps);
return(cloneGaps);
}	/*	static struct chrGapList *cloneGapList()	*/

static void bedListOutput(struct chrGapList *gapList, char *bedOutFile)
{
struct chrGapList *cl;
FILE *outFile = mustOpen(bedOutFile, "w");

verbose(2,"writing last placement to %s\n", bedOutFile);

for (cl = gapList; cl != NULL; cl = cl->next)
    {
    int chrCount = 0;
    struct gap *gl;
    struct gap *next;
    struct bed *upstreamBound = NULL;
    struct bed *downstreamBound = NULL;

    for (gl = cl->gList; gl != NULL; gl = next)
	{
	struct gap *gEl;
	int gapCount = 0;

	/*	make note of upstream and downstream bounding elements */
	if (NULL == upstreamBound || gl->isUpstreamBound)
	    {
	    if (gl->isUpstreamBound)
		{
		upstreamBound = gl->upstream;
		downstreamBound = nextDownstreamBound(gl);
		if (NULL == downstreamBound)
		    errAbort("Can not find a downstream"
			    " bounding element ?");
		}
	    else
		errAbort("Do not find a bounding element as"
		    " first upstream item ?");
	    }
	++chrCount;
	/*	output all downstream elements as long as they
	 *	are not bounding elements
	 */
	for (gEl = gl; (gEl != NULL) && (! gEl->isDownstreamBound);
		    gEl = gEl->next)
	    {
	    ++gapCount;
	    fprintf (outFile, "%s\t%d\t%d\t%s_%d.%d\n", cl->chrom,
		gEl->downstream->chromStart, gEl->downstream->chromEnd,
		cl->chrom, chrCount, gapCount);
	    }
	if (gEl)
	    next = gEl->next;
	else
	    next = NULL;
	}	/*	for (gl = cl->gList; gl != NULL; gl = next)	*/
    }	/*	for (cl=cList; cl != NULL; cl = cl->next)	*/
carefulClose(&outFile);
}	/*	static void bedListOutput()	*/

static void randomPlacement(char *bounding, char *placed)
{
struct bed *boundingElements = bedLoadAll(bounding);
struct bed *placeItems = bedLoadAll(placed);
struct bed *nearestNeighbors = NULL;
int boundingCount = slCount(boundingElements);
int placedCount = slCount(placeItems);
int neighborCount = 0;
struct chrGapList *boundingGaps = NULL;
struct chrGapList *duplicateGapList = NULL;
struct chrGapList *neighborGaps = NULL;
struct statistic *statsList = NULL;
struct statistic *statEl = NULL;

if (neighbor)
    {
    nearestNeighbors = bedLoadAll(neighbor);
    slSort(&nearestNeighbors, bedCmp);	/* order by chrom,chromStart */
    neighborCount = slCount(nearestNeighbors);
    verbose(2, "neighbor element count: %d\n", neighborCount);
    neighborGaps = createGaps(nearestNeighbors);
    }
slSort(&boundingElements, bedCmp);	/* order by chrom,chromStart */
slSort(&placeItems, bedCmp);		/* order by chrom,chromStart */

verbose(2, "bounding element count: %d\n", boundingCount);
verbose(2, "placed item count: %d\n", placedCount);

boundingGaps = createGaps(boundingElements);

if (TRUE)	/*	display initial placement stats only	*/
    {
    char *neighborName = NULL;

    if (neighbor)
	{
	neighborName = cloneString(neighbor);
	duplicateGapList = cloneGapList(neighborGaps);
	}
    else
	{
	neighborName = cloneString(bounding);
	duplicateGapList = cloneGapList(boundingGaps);
	}

    verbose(2,"stats before initial placement:  =================\n");
    statEl = gapStats(duplicateGapList, (char *)NULL);
    printf("statistics on gaps before any placements:\n\t(%s)\n", neighborName);
    statsPrint(statEl);
    slAddHead(&statsList,statEl);

    initialPlacement(duplicateGapList,placeItems);

    verbose(2,"stats after initial placement:  =================\n");
    statEl = gapStats(duplicateGapList, zeroBedOutFile);
    printf("statistics after initial placement of placed items:\n\t(%s)\n",
		placed);
    statsPrint(statEl);
    slAddHead(&statsList,statEl);

    freeChrList(&duplicateGapList, FALSE);
    slReverse(&statsList);
    freeMem(neighborName);
    }

if (trials > 0)
    {
    int trial;

    srand48((long int)seed);	/* for default seed=0, same set of randoms */

    slSort(&placeItems, bedCmpSize);	/* order by size of elements */
    slReverse(&placeItems);		/* largest ones first	*/
    measurePlaced(placeItems);		/* show placed item characteristics */
    for (trial = 0; trial < trials; ++trial)
	{
	struct bed *randomPlacedBedList;
	duplicateGapList = cloneGapList(boundingGaps);
	randomPlacedBedList = randomTrial(duplicateGapList,placeItems);
	if (neighbor)
	    {
	    struct chrGapList *duplicateNeighborList;
	    slSort(&randomPlacedBedList,bedCmp);/*order by chrom,chromStart*/
	    duplicateNeighborList = cloneGapList(neighborGaps);
	    initialPlacement(duplicateNeighborList,randomPlacedBedList);
	    statEl = gapStats(duplicateNeighborList, (char *)NULL);
	    freeChrList(&duplicateNeighborList, FALSE);
	    }
	else
	    statEl = gapStats(duplicateGapList, (char *)NULL);

	slAddHead(&statsList,statEl);
	/*	this gap list has temporary bed elements that were
	 *	created by the randomTrial(), they need to be freed as
	 *	the list is released, hence the TRUE signal.
	 *	It isn't a true freeBedList operation because the chrom
	 *	names are left intact in the original copy of the bed
	 *	list.  (The names were being shared.)
	 */
	if ((trial == (trials - 1)) && (bedOutFile != NULL))
	    {
	    bedListOutput(duplicateGapList, bedOutFile);
	    }
	freeChrList(&duplicateGapList, TRUE);
	}
    slReverse(&statsList);
    statsPrint(statsList);
    }
if (neighbor)
    {
    bedFreeList(&nearestNeighbors);
    freeChrList(&neighborGaps, FALSE);
    }
bedFreeList(&boundingElements);
bedFreeList(&placeItems);
freeChrList(&boundingGaps, FALSE);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
trials = optionInt("trials", 0);
seed = optionInt("seed", 0);
shoulder = optionInt("shoulder", 100);
neighbor = optionVal("neighbor", NULL);
bedOutFile = optionVal("bed", NULL);
zeroBedOutFile = optionVal("zeroBed", NULL);
verbosity = optionInt("verbose", 1);

verboseSetLevel(verbosity);
verbose(2,"bounding elements file: %s\n", argv[1]);
verbose(2,"placed items file: %s\n", argv[2]);
if (neighbor)
    verbose(2,"nearest neighbor file: %s\n", neighbor);
if (bedOutFile)
    verbose(2,"last alignment of trials output to bed file: %s\n", bedOutFile);
if (zeroBedOutFile)
    verbose(2,"zero distance items to bed file: %s\n", zeroBedOutFile);
verbose(2,"number of trials: %d\n", trials);
verbose(2,"seed value for drand48(): %d\n", seed);
verbose(2,"shoulder value: %d\n", shoulder);

randomPlacement(argv[1], argv[2]);
verbose(3,"at exit of main()\n");
return(0);
}
