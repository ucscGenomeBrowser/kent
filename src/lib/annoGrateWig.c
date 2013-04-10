/* annoGrateWig -- subclass of annoGrator for bigWig or wiggle data */

#include "annoGrateWig.h"
#include "annoStreamBigWig.h"

struct annoGrateWig
{
    struct annoGrator grator;		// external annoGrator interface
    struct annoGrator *mySource;	// internal annoGrator of wigDb rows
    struct lm *lm;			// localmem for rows from mySource
    int lmRowCount;			// counter for periodic cleanup
};

static void tidyUp(const struct annoRow *rowIn, struct annoRow **pOutList,
		   uint primaryStart, uint primaryEnd, struct lm *callerLm)
/* This takes a wiggle chunk coming from a .wig/database row and makes it into
 * zero or more tidy little NAN-less annoRows.  Trim rowIn to the bounds of
 * primary, trim NANs from beginning and break into multiple rows where there
 * are NANs in the middle.  If the rowIn is contiguous with the row at the
 * head of outList, expand that row to include rowIn's data. */
{
uint start = max(rowIn->start, primaryStart);
uint end = min(rowIn->end, primaryEnd);
float *vector = rowIn->data;
while (end > start)
    {
    uint offset = start - rowIn->start;
    if (vector[offset] == NAN)
	start++;
    else
	{
	// If there is a NAN before end, that's the end of this row:
	uint thisEnd = start;
	while (thisEnd < end && vector[thisEnd - rowIn->start] != NAN)
	    thisEnd++;
	struct annoRow *headRow = *pOutList;
	if (headRow == NULL || rowIn->start > headRow->end)
	    {
	    // allocate a new row
	    struct annoRow *rowOut = annoRowWigNew(rowIn->chrom, start, thisEnd, FALSE,
						   vector + offset, callerLm);
	    slAddHead(pOutList, rowOut);
	    }
	else
	    {
	    // glom new data onto headRow
	    assert(thisEnd > headRow->end);
	    uint oldEnd = headRow->end;
	    uint oldLen = oldEnd - headRow->start;
	    uint newLen = thisEnd - headRow->start;
	    headRow->data = lmAllocMoreMem(callerLm, headRow->data, oldLen*sizeof(vector[0]),
					   newLen*sizeof(vector[0]));
	    headRow->end = thisEnd;
	    float *newData = (float *)rowIn->data + (oldEnd - rowIn->start);
	    float *newSpace = (float *)headRow->data + oldLen;
	    CopyArray(newData, newSpace, (thisEnd - oldEnd));
	    }
	start = thisEnd;
	}
    }
}

static struct annoRow *agwIntegrate(struct annoGrator *gSelf, struct annoRow *primaryRow,
				    boolean *retRJFilterFailed, struct lm *callerLm)
/* Return wig annoRows that overlap primaryRow position, with NANs weeded out. */
{
struct annoGrateWig *self = (struct annoGrateWig *)gSelf;
// Cleanup internal lm every so often:
if (self->lmRowCount >= 4096)
    {
    lmCleanup(&(self->lm));
    self->lmRowCount = 0;
    }
if (self->lm == NULL)
    self->lm = lmInit(0);
struct annoRow *rowsIn = annoGratorIntegrate(self->mySource, primaryRow, retRJFilterFailed,
					     self->lm);
self->lmRowCount += slCount(rowsIn);
if (retRJFilterFailed && *retRJFilterFailed)
    return NULL;
struct annoRow *rowIn, *rowOutList = NULL;;
for (rowIn = rowsIn;  rowIn != NULL;  rowIn = rowIn->next)
    tidyUp(rowIn, &rowOutList, primaryRow->start, primaryRow->end, callerLm);
slReverse(&rowOutList);
return rowOutList;
}

struct annoGrator *annoGrateWigNew(struct annoStreamer *wigSource)
/* Create an annoGrator subclass for source with rowType == arWig. */
{
if (wigSource->rowType != arWig)
    errAbort("annoGrateWigNew: expected source->rowType arWig (%d), got %d",
	     arWig, wigSource->rowType);
struct annoGrateWig *self;
AllocVar(self);
struct annoGrator *gSelf = (struct annoGrator *)self;
annoGratorInit(gSelf, wigSource);
gSelf->integrate = agwIntegrate;
self->mySource = annoGratorNew(wigSource);
return gSelf;
}

struct annoGrator *annoGrateBigWigNew(char *fileOrUrl)
/* Create an annoGrator subclass for bigWig file or URL. */
{
struct annoStreamer *bigWigSource = annoStreamBigWigNew(fileOrUrl);
return annoGrateWigNew(bigWigSource);
}
