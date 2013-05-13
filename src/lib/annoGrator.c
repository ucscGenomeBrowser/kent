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
	errAbort("annoGrator %s: Unsorted input from primary source (%s < %s)",
		 self->streamer.name, primaryRow->chrom, self->prevPChrom);
    self->prevPChrom = cloneString(primaryRow->chrom);
    }
else if (primaryRow->start < self->prevPStart)
    errAbort("annoGrator %s:Unsorted input from primary source (%s, %u < %u)",
	     self->streamer.name, primaryRow->chrom, primaryRow->start, self->prevPStart);
self->prevPStart = primaryRow->start;
}

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
	}
    else
	prevQRow = qRow;
    }
if (self->qHead == NULL)
    {
    // Queue is empty - clean up lm
    lmCleanup(&(self->qLm));
    self->qLm = lmInit(0);
    }
}

INLINE void agCheckInternalSorting(struct annoGrator *self, struct annoRow *newRow)
/* Die if newRow precedes qTail. */
{
if (self->qTail != NULL)
    {
    int cDifNewTail = strcmp(newRow->chrom, self->qTail->chrom);
    if (cDifNewTail < 0)
	errAbort("annoGrator %s: Unsorted input from internal source %s (%s < %s)",
		 self->streamer.name, self->mySource->name, newRow->chrom, self->qTail->chrom);
    else if (cDifNewTail == 0 && newRow->start < self->qTail->start)
	errAbort("annoGrator %s: Unsorted input from internal source %s (%s, %u < %u)",
		 self->streamer.name, self->mySource->name,
		 newRow->chrom, newRow->start, self->qTail->start);
    }
}

INLINE void agFetchToEnd(struct annoGrator *self, char *chrom, uint start, uint end)
/* Fetch rows until we are sure we have all items that start to the left of end,
 * i.e. we have an item that starts at/after end or we hit eof. */
{
while (!self->eof &&
       (self->qTail == NULL || strcmp(self->qTail->chrom, chrom) < 0 || self->qTail->start < end))
    {
    struct annoRow *newRow = self->mySource->nextRow(self->mySource, chrom, start, self->qLm);
    if (newRow == NULL)
	self->eof = TRUE;
    else
	{
	agCheckInternalSorting(self, newRow);
	int cDifNewP = strcmp(newRow->chrom, chrom);
	if (cDifNewP >= 0)
	    {
	    // Add newRow to qTail
	    if (self->qTail == NULL)
		{
		if (self->qHead != NULL)
		    errAbort("annoGrator %s: qTail is NULL but qHead is non-NULL",
			     self->streamer.name);
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

struct annoRow *annoGratorIntegrate(struct annoGrator *self, struct annoStreamRows *primaryData,
				    boolean *retRJFilterFailed, struct lm *callerLm)
/* Given a single row from the primary source, get all overlapping rows from internal
 * source, and produce joined output rows.
 * If retRJFilterFailed is non-NULL:
 * - any overlapping row has a rightJoin filter failure (see annoFilter.h), or
 * - overlap rule is agoMustOverlap and no rows overlap, or
 * - overlap rule is agoMustNotOverlap and any overlapping row is found,
 * then set retRJFilterFailed and stop. */
{
struct annoRow *primaryRow = primaryData->rowList;
struct annoRow *rowList = NULL;
agCheckPrimarySorting(self, primaryRow);
agTrimToStart(self, primaryRow->chrom, primaryRow->start);
agFetchToEnd(self, primaryRow->chrom, primaryRow->start, primaryRow->end);
boolean rjFailHard = (retRJFilterFailed != NULL);
if (rjFailHard)
    *retRJFilterFailed = FALSE;
struct annoRow *qRow;
for (qRow = self->qHead;  qRow != NULL;  qRow = qRow->next)
    {
    if (qRow->start < primaryRow->end && qRow->end > primaryRow->start &&
	sameString(qRow->chrom, primaryRow->chrom))
	{
	int numCols = self->mySource->numCols;
	enum annoRowType rowType = self->mySource->rowType;
	slAddHead(&rowList, annoRowClone(qRow, rowType, numCols, callerLm));
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
if (retRJFilterFailed &&
    ((rowList == NULL && (self->haveRJIncludeFilter || self->overlapRule == agoMustOverlap)) ||
     (rowList != NULL && self->overlapRule == agoMustNotOverlap)))
    *retRJFilterFailed = TRUE;
return rowList;
}

void annoGratorClose(struct annoStreamer **pSelf)
/* Free self (including mySource). */
{
if (pSelf == NULL)
    return;
struct annoGrator *self = *(struct annoGrator **)pSelf;
lmCleanup(&(self->qLm));
self->mySource->close(&(self->mySource));
freeMem(self->prevPChrom);
freez(pSelf);
}

static struct annoRow *noNextRow(struct annoStreamer *self, char *minChrom, uint minEnd,
				 struct lm *callerLm)
/* nextRow() is N/A for annoGrator, which needs caller to use integrate() instead. */
{
errAbort("annoGrator %s: nextRow() called, but integrate() should be called instead",
	 self->name);
return NULL;
}

static void agReset(struct annoGrator *self)
/* Reset all state associated with position */
{
freez(&(self->prevPChrom));
self->prevPStart = 0;
self->eof = FALSE;
lmCleanup(&(self->qLm));
self->qLm = lmInit(0);
self->qHead = self->qTail = NULL;
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

static void agSetAutoSqlObject(struct annoStreamer *sSelf, struct asObject *asObj)
/* Use new asObj and update internal state derived from asObj. */
{
struct annoGrator *gSelf = (struct annoGrator *)sSelf;
annoStreamerSetAutoSqlObject(sSelf, asObj);
gSelf->haveRJIncludeFilter = filtersHaveRJInclude(sSelf->filters);
}

void agSetOverlapRule(struct annoGrator *self, enum annoGratorOverlap rule)
/* Tell annoGrator how to handle overlap of its rows with primary row. */
{
self->overlapRule = rule;
}

void annoGratorInit(struct annoGrator *self, struct annoStreamer *mySource)
/* Initialize an integrator of columns from mySource with (positions of)
 * rows passed to integrate().
 * mySource becomes property of the annoGrator. */
{
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, mySource->assembly, mySource->getAutoSqlObject(mySource),
		 mySource->name);
streamer->rowType = mySource->rowType;
streamer->setAutoSqlObject = agSetAutoSqlObject;
streamer->setFilters = agSetFilters;
streamer->setRegion = annoGratorSetRegion;
streamer->nextRow = noNextRow;
streamer->close = annoGratorClose;
self->qLm = lmInit(0);
self->integrate = annoGratorIntegrate;
self->setOverlapRule = agSetOverlapRule;
self->overlapRule = agoNoConstraint;
self->mySource = mySource;
self->haveRJIncludeFilter = filtersHaveRJInclude(streamer->filters);
}

struct annoGrator *annoGratorNew(struct annoStreamer *mySource)
/* Make a new integrator of columns from mySource with (positions of) rows passed to integrate().
 * mySource becomes property of the new annoGrator. */
{
struct annoGrator *self;
AllocVar(self);
annoGratorInit(self, mySource);
return self;
}
