/* annoGrator -- join two inputs on position, keeping all original fields intact. */

#include "annoGrator.h"

static void agCheckPrimarySorting(struct annoGrator *self, struct annoRow *primaryRow)
/* Die if primaryRow seems to have arrived out of order. */
{
if (self->prevPChrom == NULL || differentString(primaryRow->chrom, self->prevPChrom))
    self->prevPChrom = cloneString(primaryRow->chrom);
else if (primaryRow->start < self->prevPStart)
    errAbort("Unsorted input from primarySource (%s, %u < %u)",
	     primaryRow->chrom, primaryRow->start, self->prevPStart);
self->prevPStart = primaryRow->start;
}

//#*** use localmem for queue? one per chrom?  free when empty?  reuse structs?

INLINE void agTrimToStart(struct annoGrator *self, char *chrom, uint start)
/* If queue contains items whose end is to the left of start, splice them out. */
{
struct annoRow *qRow, *prevQRow = NULL, *nextQRow;
for (qRow = self->qHead;  qRow != NULL;  qRow = nextQRow)
    {
    nextQRow = qRow->next;
    if (sameString(qRow->chrom, chrom) && qRow->start >= start)
	break;
    else if (differentString(qRow->chrom, chrom) || qRow->end < start)
	{
	if (prevQRow == NULL)
	    self->qHead = qRow->next;
	else
	    prevQRow->next = qRow->next;
	if (self->qTail == qRow)
	    self->qTail = prevQRow;
	//#*** free/discard/reuse qRow here
	}
    else
	prevQRow = qRow;
    }
}

INLINE void agFetchToEnd(struct annoGrator *self, char *chrom, uint end)
/* Fetch rows until we are sure we have all items that start to the left of end,
 * i.e. we have an item that starts at/after end or we hit eof. */
{
while (!self->eof && (self->qTail == NULL ||
		      (sameString(self->qTail->chrom, chrom) && self->qTail->start < end)))
    {
    struct annoRow *newRow = self->mySource->nextRow(self->mySource);
    if (newRow == NULL)
	self->eof = TRUE;
    else
	{
	if (self->qTail != NULL)
	    {
	    if (sameString(self->qTail->chrom, chrom) && newRow->start < self->qTail->start)
		errAbort("unsorted input from internal source (%s, %u < %u)",
			 chrom, newRow->start, self->qTail->start);
	    self->qTail->next = newRow;
	    }
	self->qTail = newRow;
	if (self->qHead == NULL)
	    self->qHead = newRow;
	}
    }
}

struct annoRow *annoGratorIntegrate(struct annoGrator *self, struct annoRow *primaryRow,
				    boolean *retRJFilterFailed)
/* Given a single row from the primary source, get all overlapping rows from internal
 * source, and produce joined output rows.  If retRJFilterFailed is non-NULL and any 
 * overlapping row has a rightJoin filter failure (see annoFilter.h),
 * set retRJFilterFailed and stop. */
{
struct annoRow *rowList = NULL;
agCheckPrimarySorting(self, primaryRow);
agTrimToStart(self, primaryRow->chrom, primaryRow->start);
agFetchToEnd(self, primaryRow->chrom, primaryRow->end);
boolean rjFailHard = (retRJFilterFailed != NULL);
int numCols = slCount(self->mySource->columns);
struct annoRow *qRow;
for (qRow = self->qHead;  qRow != NULL;  qRow = qRow->next)
    {
    if (qRow->start < primaryRow->end && qRow->end > primaryRow->start)
	{
	slAddHead(&rowList, annoRowClone(qRow, numCols));
	if (rjFailHard && qRow->rightJoinFail)
	    {
	    *retRJFilterFailed = TRUE;
	    break;
	    }
	}
    }
slReverse(&rowList);
return rowList;
}

void annoGratorGenericClose(struct annoStreamer **pSelf)
/* Free self (including mySource). */
{
if (pSelf == NULL)
    return;
struct annoStreamer *mySource = (*(struct annoGrator **)pSelf)->mySource;
mySource->close(&mySource);
freez(pSelf);
}

static struct annoRow *noNextRow(struct annoStreamer *self)
/* nextRow() is N/A for annoGrator, which needs caller to use integrate() instead. */
{
errAbort("nextRow() called on annoGrator object, but integrate() should be called instead");
return NULL;
}

static void agReset(struct annoGrator *self)
/* Reset all position associated with state */
{
freez(&self->prevPChrom);
self->prevPStart = 0;
self->eof = FALSE;
//#*** free/discard/reuse q here
self->qHead = self->qTail = NULL;
}

void annoGratorSetRegion(struct annoStreamer *vSelf, char *chrom, uint rStart, uint rEnd)
/* Set genomic region for query, and reset internal state. */
{
struct annoGrator *self = (struct annoGrator *)vSelf;
self->mySource->setRegion((struct annoStreamer *)(self->mySource), chrom, rStart, rEnd);
agReset(self);
}

void annoGratorSetQuery(struct annoStreamer *vSelf, struct annoGratorQuery *query)
/* Set query (to be called only by annoGratorQuery which is created after streamers). */
{
struct annoGrator *self = (struct annoGrator *)vSelf;
self->streamer.query = query;
self->mySource->setQuery((struct annoStreamer *)(self->mySource), query);
}

struct annoGrator *annoGratorGenericNew(struct annoStreamer *mySource)
/* Make a new integrator of columns from two annoStreamer sources.
 * mySource becomes property of the new annoGrator. */
{
struct annoGrator *self;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, mySource->getAutoSqlObject(mySource));
streamer->setRegion = annoGratorSetRegion;
streamer->setQuery = annoGratorSetQuery;
streamer->nextRow = noNextRow;
streamer->close = annoGratorGenericClose;
self->integrate = annoGratorIntegrate;
self->mySource = mySource;
return self;
}
