/* annoGrator -- join two inputs on position, keeping all original fields intact. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
    errAbort("annoGrator %s: Unsorted input from primary source (%s, %u < %u)",
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
    self->qSkippedCount = 0;
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
       (self->qTail == NULL || strcmp(self->qTail->chrom, chrom) < 0 ||
	(sameString(self->qTail->chrom, chrom) && self->qTail->start <= end)))
    {
    // If primary row is a zero-length insertion, we will want to pick up items adjacent
    // to it on the left, so move minEnd one base to the left:
    uint minEnd = (start == end) ? start - 1 : start;
    struct annoRow *newRow = self->mySource->nextRow(self->mySource, chrom, minEnd, self->qLm);
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
	// If we're skipping past large regions, keep qLm size under control:
	else
	    {
	    self->qSkippedCount++;
	    if (self->qSkippedCount > 1024 && self->qHead == NULL && self->qTail == NULL)
		{
		lmCleanup(&(self->qLm));
		self->qLm = lmInit(0);
		self->qSkippedCount = 0;
		}
	    }
	}
    }
}

static boolean handleRJFail(boolean rjFail, boolean *retRJFilterFailed)
/* If retRJFilterFailed is non-NULL and rjFail is TRUE then set *retRJFilterFailed to TRUE
 * and return TRUE.  Otherwise do nothing and return FALSE. */
{
if (retRJFilterFailed != NULL && rjFail)
    {
    *retRJFilterFailed = TRUE;
    return TRUE;
    }
return FALSE;
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
int pStart = primaryRow->start, pEnd = primaryRow->end;
char *pChrom = primaryRow->chrom;
agTrimToStart(self, pChrom, pStart);
agFetchToEnd(self, pChrom, pStart, pEnd);
boolean rjFailHard = (retRJFilterFailed != NULL);
if (rjFailHard)
    *retRJFilterFailed = FALSE;
int numCols = self->mySource->numCols;
enum annoRowType rowType = self->mySource->rowType;
struct annoRow *qRow;
if (pStart == pEnd)
    {
    // If the primary feature is an insertion, catch any feature that is adjacent to it,
    // not only those that overlap.
    for (qRow = self->qHead;  qRow != NULL;  qRow = qRow->next)
        {
        if (qRow->start <= pEnd && qRow->end >= pStart)
            {
            slAddHead(&rowList, annoRowClone(qRow, rowType, numCols, callerLm));
            if (handleRJFail(qRow->rightJoinFail, retRJFilterFailed))
                break;
            }
        }
    }
else
    {
    for (qRow = self->qHead;  qRow != NULL;  qRow = qRow->next)
        {
        if (((qRow->start < pEnd && qRow->end > pStart) ||
             // Make sure to include q insertions at pStart or pEnd:
             (qRow->start == qRow->end &&
              (qRow->start == pEnd || qRow->end == pStart))) &&
            sameString(qRow->chrom, pChrom))
            {
            slAddHead(&rowList, annoRowClone(qRow, rowType, numCols, callerLm));
            if (handleRJFail(qRow->rightJoinFail, retRJFilterFailed))
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
self->qSkippedCount = 0;
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
/* Replace filters and re-evaluate self->haveRJIncludeFilter. Apply filters to
 * own streamer interface and to internal source. */
{
annoStreamerSetFilters(vSelf, newFilters);
struct annoGrator *self = (struct annoGrator *)vSelf;
self->haveRJIncludeFilter = filtersHaveRJInclude(vSelf->filters);
self->mySource->setFilters(self->mySource, newFilters);
}

static void agAddFilters(struct annoStreamer *vSelf, struct annoFilter *newFilters)
/* Add filters and re-evaluate self->haveRJIncludeFilter. Apply filters to
 * own streamer interface and to internal source. */
{
annoStreamerAddFilters(vSelf, newFilters);
struct annoGrator *self = (struct annoGrator *)vSelf;
annoStreamerSetFilters(self->mySource, vSelf->filters);
self->haveRJIncludeFilter = filtersHaveRJInclude(vSelf->filters);
}

void annoGratorSetRegion(struct annoStreamer *vSelf, char *chrom, uint rStart, uint rEnd)
/* Set genomic region for query, and reset internal state. */
{
annoStreamerSetRegion(vSelf, chrom, rStart, rEnd);
struct annoGrator *self = (struct annoGrator *)vSelf;
self->mySource->setRegion((struct annoStreamer *)(self->mySource), chrom, rStart, rEnd);
agReset(self);
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
streamer->setFilters = agSetFilters;
streamer->addFilters = agAddFilters;
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

void annoGratorSetAutoSqlObject(struct annoStreamer *sSelf, struct asObject *asObj)
/* Use new asObj and update internal state derived from asObj. */
{
struct annoGrator *gSelf = (struct annoGrator *)sSelf;
annoStreamerSetAutoSqlObject(sSelf, asObj);
gSelf->haveRJIncludeFilter = filtersHaveRJInclude(sSelf->filters);
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
