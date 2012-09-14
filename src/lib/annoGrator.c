/* annoGrator -- join two inputs on position, keeping all original fields intact. */

#include "annoGrator.h"

INLINE void agCheckPrimarySorting(struct annoGrator *self, struct annoRow *primaryRow)
/* Die if primaryRow seems to have arrived out of order. */
{
if (self->prevPChrom == NULL)
    self->prevPChrom = cloneString(primaryRow->chrom);
else if (differentString(primaryRow->chrom, self->prevPChrom))
    {
    if (strcmp(primaryRow->chrom, self->prevPChrom) < 0)
	errAbort("Unsorted input from primarySource (%s < %s)",
		 primaryRow->chrom, self->prevPChrom);
    self->prevPChrom = cloneString(primaryRow->chrom);
    }
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
    int cDifRowP = strcmp(qRow->chrom, chrom);
    if (cDifRowP > 0 || (cDifRowP == 0 && qRow->start >= start))
	break;
    else if (cDifRowP < 0 || qRow->end < start)
	{
	if (prevQRow == NULL)
	    self->qHead = qRow->next;
	else
	    prevQRow->next = qRow->next;
	if (self->qTail == qRow)
	    self->qTail = prevQRow;
	annoRowFree(&qRow, self->mySource);
	}
    else
	prevQRow = qRow;
    }
}

INLINE void agCheckInternalSorting(struct annoRow *newRow, struct annoRow *qTail)
/* Die if newRow precedes qTail. */
{
if (qTail != NULL)
    {
    int cDifNewTail = strcmp(newRow->chrom, qTail->chrom);
    if (cDifNewTail < 0)
	errAbort("Unsorted input from internal source (%s < %s)",
		 newRow->chrom, qTail->chrom);
    else if (cDifNewTail == 0 && newRow->start < qTail->start)
	errAbort("Unsorted input from internal source (%s, %u < %u)",
		 newRow->chrom, newRow->start, qTail->start);
    }
}

INLINE void agFetchToEnd(struct annoGrator *self, char *chrom, uint end)
/* Fetch rows until we are sure we have all items that start to the left of end,
 * i.e. we have an item that starts at/after end or we hit eof. */
{
while (!self->eof &&
       (self->qTail == NULL || strcmp(self->qTail->chrom, chrom) < 0 || self->qTail->start < end))
    {
    struct annoRow *newRow = self->mySource->nextRow(self->mySource);
    if (newRow == NULL)
	self->eof = TRUE;
    else
	{
	agCheckInternalSorting(newRow, self->qTail);
	int cDifNewP = strcmp(newRow->chrom, chrom);
	if (cDifNewP < 0)
	    // newRow->chrom comes before chrom; skip over newRow
	    annoRowFree(&newRow, (struct annoStreamer *)self);
	else
	    {
	    // Add newRow to qTail
	    if (self->qTail == NULL)
		{
		if (self->qHead != NULL)
		    errAbort("qTail is NULL but qHead is non-NULL");
		self->qHead = self->qTail = newRow;
		}
	    else
		{
		self->qTail->next = newRow;
		self->qTail = newRow;
		}
	    if (cDifNewP > 0)
		// newRow->chrom comes after chrom; we're done for now
		break;
	    }
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
if (rjFailHard)
    *retRJFilterFailed = FALSE;
struct annoRow *qRow;
for (qRow = self->qHead;  qRow != NULL;  qRow = qRow->next)
    {
    if (qRow->start < primaryRow->end && qRow->end > primaryRow->start &&
	sameString(qRow->chrom, primaryRow->chrom))
	{
	slAddHead(&rowList, annoRowClone(qRow, self->mySource));
	if (rjFailHard && qRow->rightJoinFail)
	    {
	    *retRJFilterFailed = TRUE;
	    break;
	    }
	}
    }
slReverse(&rowList);
// If no rows overlapped primary, and there is a right-join, !isExclude (i.e. isInclude) filter,
// then we need to set retRJFilterFailed because the condition was not met to include
// the primary item.
if (rowList == NULL && rjFailHard && self->haveRJIncludeFilter)
    *retRJFilterFailed = TRUE;
return rowList;
}

void annoGratorClose(struct annoStreamer **pSelf)
/* Free self (including mySource). */
{
if (pSelf == NULL)
    return;
struct annoGrator *self = *(struct annoGrator **)pSelf;
annoRowFreeList(&(self->qHead), self->mySource);
self->mySource->close(&(self->mySource));
freeMem(self->prevPChrom);
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
annoRowFreeList(&(self->qHead), (struct annoStreamer *)self);
self->qTail = NULL;
}

static boolean filtersHaveRJInclude(struct annoFilter *filters)
/* Return TRUE if filters have at least one active filter with !isExclude && rightJoin. */
{
struct annoFilter *filter;
for (filter = filters;  filter != NULL;  filter = filter->next)
    if (filter->op != afNoFilter && !filter->isExclude && filter->rightJoin)
	return TRUE;
return FALSE;
}

static void agSetFilters(struct annoStreamer *vSelf, struct annoFilter *newFilters)
/* Update filters and re-evaluate self->haveRJIncludeFilter */
{
annoStreamerSetFilters(vSelf, newFilters);
struct annoGrator *self = (struct annoGrator *)vSelf;
self->haveRJIncludeFilter = filtersHaveRJInclude(vSelf->filters);
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

struct annoGrator *annoGratorNew(struct annoStreamer *mySource)
/* Make a new integrator of columns from mySource with (positions of) rows passed to integrate().
 * mySource becomes property of the new annoGrator. */
{
struct annoGrator *self;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, mySource->getAutoSqlObject(mySource));
streamer->rowType = mySource->rowType;
streamer->setFilters = agSetFilters;
streamer->setRegion = annoGratorSetRegion;
streamer->setQuery = annoGratorSetQuery;
streamer->nextRow = noNextRow;
streamer->close = annoGratorClose;
self->integrate = annoGratorIntegrate;
self->mySource = mySource;
self->haveRJIncludeFilter = filtersHaveRJInclude(streamer->filters);
return self;
}
