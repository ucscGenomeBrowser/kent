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

struct gapList
    {
    struct gapList *next;	/*	double linked list	*/
    struct gapList *prev;
    struct bed *upstream;	/*	bounding item upstream of this gap */
    struct bed *downstream;	/*	bounding item downstream of this gap */
    int gapSize;		/* downstream->chromStart - upstream->chromEnd*/
    boolean upstreamType;	/*	TRUE == bounding item	*/
    boolean downstreamType;	/*	FALSE == placed item	*/
    };

static void freeGapList(struct gapList **gl)
{
struct gapList *el, *next;
for (el = *gl; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el);
    }
*gl = NULL;
}

struct chrList
/*	a list of gapLists, one element for each chrom	*/
    {
    struct chrList *next;
    char *chrom;
    struct gapList *gList;
    };

static void freeChrList(struct chrList **cl)
{
struct chrList *el, *next;

for (el = *cl; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el->chrom);
    freeGapList(&el->gList);
    freeMem(el);
    }
*cl = NULL;
}

static void gapStats(struct chrList *cList)
{
struct chrList *cl;
int chrCount = 0;
int gapCountNonZeroSize = 0;
int gapCountZeroSize = 0;
int totalGapSize = 0;
int averageGapSize = 0;
int maxGap = 0;
int minGap = BIGNUM;
float *gapSizeArray = NULL;
int i;

/*	first count number of non-zero gaps	*/
for (cl=cList; cl != NULL; cl = cl->next)
    {
    struct gapList *gl;
    int gapCount = 0;
    int zeroSized = 0;
verbose(3,"chrom: %s: %d gaps\n", cl->chrom, slCount(cl->gList));
    for (gl = cl->gList; gl != NULL; gl = gl->next)
	{
	int gapSize = gl->gapSize;
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

/*	now copy all the values to a float array for more detailed
 *	stats measurements
 */
gapSizeArray = needHugeMem((size_t)(sizeof(float) * gapCountNonZeroSize));
i = 0;

for (cl=cList; cl != NULL; cl = cl->next)
    {
    struct gapList *gl;
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
verbose(3,"assigned %d values to float array\n", i);

floatSort(i,gapSizeArray);

averageGapSize = 0.5 + (double)totalGapSize/(double)gapCountNonZeroSize;
verbose(2,"average gap size: %d = %d / %d (non-zero size gaps only)\n",
    averageGapSize, totalGapSize, gapCountNonZeroSize);
verbose(2,"maximum gap size: %f\n", gapSizeArray[i-1]);
verbose(2,"median gap size: %f\n", gapSizeArray[i/2]);
verbose(2,"minimum gap size: %f\n", gapSizeArray[0]);
verbose(2,"minimum gap: %d, maximum gap: %d\n", minGap, maxGap);
freeMem(gapSizeArray);
}

static void placeItem(struct bed *bedEl, struct gapList *gl)
/*	create two gaps where one now exists, the bedEl splits
 *	the existing gap (== gl) which becomes the new downstream gap.
 */
{
struct gapList *el;
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

el->next = gl->next;	/*	could be NULL	*/
el->prev = gl;		/*	is NEVER NULL	*/
el->upstream = bedEl;	/*	this element is the new gap's upstream */
el->upstreamType = FALSE;	/*	not a bounding element	*/
el->downstream = gl->downstream;	/*	can NEVER be NULL	*/
el->downstreamType = gl->downstreamType;
gl->next->prev = el;
gl->downstream = bedEl;
gl->downstreamType = FALSE;	/*	not a bounding element	*/
gl->next = el;

gapSize = el->downstream->chromStart - bedEl->chromEnd;
if (gapSize >= 0)
    el->gapSize = gapSize;
else
    {
    warn("WARNING: new element insert overlaps following element\n");
    el->gapSize = 0;
    }
gapSize = bedEl->chromStart - gl->upstream->chromEnd;
if (gapSize >= 0)
    el->prev->gapSize = gapSize;
else
    {
    warn("WARNING: new element insert overlaps previous element\n");
    el->prev->gapSize = 0;
    }
}

static void initialPlacement(struct chrList *bounding, struct bed *placed)
{
struct bed *bedEl;
int unplacedCount = 0;

for (bedEl = placed; bedEl != NULL; bedEl = bedEl->next)
    {
    struct chrList *cl;
    boolean placedOK = FALSE;
    for (cl = bounding; cl != NULL; cl = cl->next)
	{
	struct gapList *gl;
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
verbose(3,"item: %s:%d-%d, gap: %s:%d-%d\n",
	bedEl->chrom, bedEl->chromStart, bedEl->chromEnd,
	bedEl->chrom, gl->upstream->chromEnd, gl->downstream->chromStart);
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

static void randomPlacement(char *bounding, char *placed)
{
struct bed *boundingElements = bedLoadAll(bounding);
struct bed *placedItems = bedLoadAll(placed);
struct bed *bedEl = NULL;
int boundingCount = slCount(boundingElements);
int placedCount = slCount(placedItems);
char *prevChr = NULL;
struct chrList *boundingChrList = NULL;
int boundingChrCount = 0;
struct gapList *prevGap = NULL;
struct bed *prevBedEl = NULL;
struct chrList *curChrList = NULL;
int overlappedBounding = 0;

verbose(3,"boundingChrList at: %#x\n", (unsigned) boundingChrList);

slSort(&boundingElements, bedCmp);	/* order by chrom,chromStart */
slSort(&placedItems, bedCmp);		/* order by chrom,chromStart */

verbose(2, "bounding element count: %d\n", boundingCount);
verbose(2, "placed item count: %d\n", placedCount);

for (bedEl = boundingElements; bedEl != NULL; bedEl = bedEl->next)
    {
    /*	the first bedEl does not yet start a new gap, must have a second */
    if ((NULL == prevChr) || differentWord(prevChr,bedEl->chrom))
	{
	struct chrList *cEl;
	AllocVar(cEl);
	cEl->chrom = cloneString(bedEl->chrom);
	cEl->gList = NULL;
	prevGap = NULL;
	prevBedEl = bedEl;	/*	bounding element before first gap */
	if (prevChr)
	    freeMem(prevChr);
	prevChr = cloneString(bedEl->chrom);
	verbose(4,"new chrom on bounding gapList creation %s\n", prevChr);
	slAddHead(&boundingChrList,cEl);
	++boundingChrCount;
	curChrList = cEl;
	}
    else
	{
	struct gapList *gEl;
	AllocVar(gEl);
	gEl->prev = prevGap;	/*	first one is NULL	*/
	gEl->upstream = prevBedEl;
	gEl->upstreamType = TRUE;	/*	bounding element	*/
	gEl->downstream = bedEl;
	gEl->downstreamType = TRUE;	/*	bounding element */
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
slReverse(&boundingChrList);
verbose(3,"bounding chrom count: %d (%d), overlapped items: %d\n",
	boundingChrCount, slCount(boundingChrList), overlappedBounding);
if (0 == trials)
   {
   verbose(2,"stats before initial placement:  =================\n");
   gapStats(boundingChrList);
   initialPlacement(boundingChrList,placedItems);
   verbose(2,"stats after initial placement:  =================\n");
   gapStats(boundingChrList);
   }
else
   {
   gapStats(boundingChrList);
   }
bedFreeList(&boundingElements);
bedFreeList(&placedItems);
freeChrList(&boundingChrList);
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
