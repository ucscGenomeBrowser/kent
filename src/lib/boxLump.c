/* boxLump - This will lump together boxes that overlap into the smallest
 * box that encompasses the overlap.  It will put other boxes that 
 * fit in the encompassing box in there too. 
 *   It works by projecting the box list along one dimension at a
 * time looking for gaps between boxes. This is similar in function
 * to boxFindClumps, but a bit less precise, and quite a bit faster.
 * in some important cases. */

#include "common.h"
#include "hash.h"
#include "boxClump.h"
#include "boxLump.h"

static char const rcsid[] = "$Id: boxLump.c,v 1.2 2008/09/17 17:56:37 kent Exp $";

int boxInCmpQuery(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct boxIn *a = *((struct boxIn **)va);
const struct boxIn *b = *((struct boxIn **)vb);
return a->qStart - b->qStart;
}

int boxInCmpTarget(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct boxIn *a = *((struct boxIn **)va);
const struct boxIn *b = *((struct boxIn **)vb);
return a->tStart - b->tStart;
}

struct boxClump *lumpOneDimension(struct boxIn *boxList, boolean onQuery)
/* Build box clump list on one dimension. */
{
struct boxClump *clump = NULL, *clumpList = NULL;
struct boxIn *box, *nextBox;
if (onQuery)
    slSort(&boxList, boxInCmpQuery);
else
    slSort(&boxList, boxInCmpTarget);
for (box = boxList; box != NULL; box = nextBox)
    {
    nextBox = box->next;
    /* Make new clump containing current box. */
    if (clump == NULL || 
        (onQuery && clump->qEnd < box->qStart) ||
	(!onQuery && clump->tEnd < box->tStart) )
        {
	AllocVar(clump);
	slAddHead(&clumpList, clump);
	clump->qStart = box->qStart;
	clump->qEnd = box->qEnd;
	clump->tStart = box->tStart;
	clump->tEnd = box->tEnd;
	clump->boxCount = 1;
	clump->boxList = box;
	box->next = NULL;
	}
    else
        {
	if (clump->tStart > box->tStart)
	     clump->tStart = box->tStart;
	if (clump->tEnd < box->tEnd)
	     clump->tEnd = box->tEnd;
	if (clump->qEnd < box->qEnd)
	     clump->qEnd = box->qEnd;
	if (clump->qStart > box->qStart)
	     clump->qStart = box->qStart;
	clump->boxCount += 1;
	slAddHead(&clump->boxList, box);
	}
    }
return clumpList;
}

struct boxClump *boxLump(struct boxIn **pBoxList)
/* Convert list of boxes to a list of lumps.  The lumps
 * are a smaller number of boxes that between them contain
 * all of the input boxes.  Note that
 * the original boxList is overwritten as the boxes
 * are moved from it to the lumps. */
{
struct boxClump *qClumpList = NULL, *tClumpList = NULL, 
	*tClump;

if (*pBoxList == NULL)
    return NULL;
tClumpList = lumpOneDimension(*pBoxList, FALSE);

for (tClump = tClumpList; tClump != NULL; tClump = tClump->next)
    {
    struct boxClump *oneList = lumpOneDimension(tClump->boxList, TRUE);
    if (slCount(oneList) > 1)
        {
	struct boxClump *clump;
	for (clump = oneList; clump != NULL; clump = clump->next)
	    {
	    struct boxClump *subList = boxLump(&clump->boxList);
	    qClumpList = slCat(subList, qClumpList);
	    }
	boxClumpFreeList(&oneList);
	}
    else
	{
	qClumpList = slCat(oneList, qClumpList);
	}
    tClump->boxList = NULL;
    }

boxClumpFreeList(&tClumpList);
*pBoxList = NULL;
return qClumpList;
}

#ifdef DEBUG

int testData[][4] = 
    {
	/* qStart, qEnd, tStart, tEnd */
	{0, 100,     0, 100},
	{50, 150,    50, 150},
	{200, 300,   200, 300},
	{250, 350,   250, 350},
	{0, 100,     200, 300},
	{50, 150,    250, 350},
	{200,300,    0, 100},
	{250,350,    50, 150},
	{500,600,    100,300},
	{500,600,    500, 600},
	{500,700,    500, 700},
	{1000,1100,  1000,1100},
	{1000,1100,  2000,2100},
    };


void testBoxLump()
/* Test boxLump routine. */
{
struct boxIn *boxList = NULL, *box;
struct boxClump *clumpList, *clump;
int i;

for (i=0; i<ArraySize(testData); ++i)
    {
    AllocVar(box);
    box->qStart = testData[i][0];
    box->qEnd = testData[i][1];
    box->tStart = testData[i][2];
    box->tEnd = testData[i][3];
    slAddHead(&boxList, box);
    }
clumpList = boxLump(&boxList);
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    printf("%d,%d,\t%d,%d\n", clump->qStart, clump->qEnd, clump->tStart, clump->tEnd);
    for (box = clump->boxList; box != NULL; box = box->next)
	printf("\t%d,%d\t%d,%d\n", box->qStart, box->qEnd, box->tStart, box->tEnd);
    }
}
#endif /* DEBUG */
