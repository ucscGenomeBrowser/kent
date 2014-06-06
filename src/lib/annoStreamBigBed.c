/* annoStreamBigBed -- subclass of annoStreamer for bigBed file or URL */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamBigBed.h"
#include "bigBed.h"
#include "localmem.h"
#include "sqlNum.h"

struct annoStreamBigBed
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    struct bbiFile *bbi;		// bbi handle for bigBed file/URL.
    struct lm *intervalQueryLm;		// localmem object for bigBedIntervalQuery
    struct bigBedInterval *intervalList;	// results of bigBedIntervalQuery
    struct bigBedInterval *nextInterval;	// next result to be translated into row
    struct bbiChromInfo *chromList;	// list of chromosomes for which bbi actually has data
    struct bbiChromInfo *queryChrom;	// most recently queried chrom for whole-genome (or NULL)
    boolean useMaxItems;		// TRUE if maxItems passed to annoStreamBigBedNew is > 0
    int maxItems;			// max remaining annoRows that nextRow can return
    boolean eof;			// TRUE when we are done (maxItems or no more items)
    boolean doNextChunk;		// TRUE if intervalList ends before end of chrom/region
    uint nextChunkStart;		// Start coord for next chunk of intervals to query
    char **row;				// storage for results of bigBedIntervalToRow
    char *startBuf;			// storage for stringified start from bigBedIntervalToRow
    char *endBuf;			// storage for stringified end from bigBedIntervalToRow
    };

// For performance reasons, even if !useMaxItems (no limit), we need to limit the
// number of intervals that are slurped into memory for a bigBedIntervalQuery, so
// we don't sit waiting too long when a chromosome has millions of intervals.
#define ASBB_CHUNK_SIZE 100000

static void asbbSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free localmem from previous query if necessary. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamBigBed *self = (struct annoStreamBigBed *)vSelf;
self->nextInterval = self->intervalList = NULL;
self->queryChrom = NULL;
self->eof = FALSE;
self->doNextChunk = FALSE;
lmCleanup(&(self->intervalQueryLm));
}

static void updateNextChunkState(struct annoStreamBigBed *self, int queryMaxItems)
/* If the just-fetched interval list was limited to ASBB_CHUNK_SIZE, set doNextChunk
 * and trim the last interval(s) so that when we query the next chunk, we don't get
 * repeat rows due to querying a start coord that was already returned. */
{
if (queryMaxItems == ASBB_CHUNK_SIZE)
    {
    int itemCount = slCount(self->intervalList);
    if (itemCount == ASBB_CHUNK_SIZE)
	{
	self->doNextChunk = TRUE;
	struct bigBedInterval *lastIv = NULL, *iv;
	for (iv = self->intervalList;  iv->next != NULL;  iv = iv->next)
	    {
	    if (iv->start != iv->next->start)
		{
		lastIv = iv;
		self->nextChunkStart = iv->next->start;
		}
	    }
	lastIv->next = NULL;
	}
    else
	self->doNextChunk = FALSE;
    }
else
    self->doNextChunk = FALSE;
}

static void asbbDoQuery(struct annoStreamBigBed *self, char *minChrom, uint minEnd)
/* Store results of an interval query. */
{
struct annoStreamer *sSelf = &(self->streamer);
if (sSelf->chrom != NULL && self->intervalList != NULL && !self->doNextChunk)
    // We're doing a region query, we already got some rows, and don't need another chunk:
    self->eof = TRUE;
if (self->useMaxItems)
    {
    int lastIntervalCount = slCount(self->intervalList);
    self->maxItems -= lastIntervalCount;
    if (self->maxItems <= 0)
	self->eof = TRUE;
    }
self->nextInterval = self->intervalList = NULL;
lmCleanup(&self->intervalQueryLm);
if (self->eof)
    return;
self->intervalQueryLm = lmInit(0);
int queryMaxItems = ASBB_CHUNK_SIZE;
if (self->useMaxItems && self->maxItems < queryMaxItems)
    queryMaxItems = self->maxItems;
if (sSelf->chrom != NULL)
    {
    uint start = sSelf->regionStart;
    if (minChrom)
	{
	if (differentString(minChrom, sSelf->chrom))
	    errAbort("annoStreamBigBed %s: nextRow minChrom='%s' but region chrom='%s'",
		     sSelf->name, minChrom, sSelf->chrom);
	if (start < minEnd)
	    start = minEnd;
	}
    if (self->doNextChunk && start < self->nextChunkStart)
	start = self->nextChunkStart;
    self->intervalList = bigBedIntervalQuery(self->bbi, sSelf->chrom, start, sSelf->regionEnd,
					     queryMaxItems, self->intervalQueryLm);
    updateNextChunkState(self, queryMaxItems);
    }
else
    {
    // Genome-wide query: break it into chrom-by-chrom queries.
    if (self->queryChrom == NULL)
	self->queryChrom = self->chromList;
    else if (!self->doNextChunk)
	self->queryChrom = self->queryChrom->next;
    if (minChrom != NULL)
	{
	// Skip chroms that precede minChrom
	while (self->queryChrom != NULL && strcmp(self->queryChrom->name, minChrom) < 0)
	    {
	    self->queryChrom = self->queryChrom->next;
	    self->doNextChunk = FALSE;
	    }
	}
    if (self->queryChrom == NULL)
	{
	self->eof = TRUE;
	}
    else
	{
	char *chrom = self->queryChrom->name;
	int start = 0;
	if (minChrom != NULL && sameString(chrom, minChrom))
	    start = minEnd;
	if (self->doNextChunk && start < self->nextChunkStart)
	    {
	    start = self->nextChunkStart;
	    }
	uint end = self->queryChrom->size;
	self->intervalList = bigBedIntervalQuery(self->bbi, chrom, start, end,
						 queryMaxItems, self->intervalQueryLm);
	updateNextChunkState(self, queryMaxItems);
	}
    }
self->nextInterval = self->intervalList;
}

static char **nextRowUnfiltered(struct annoStreamBigBed *self, char *minChrom, uint minEnd)
/* Convert the next available interval into a row of words, or return NULL. */
{
struct annoStreamer *sSelf = &(self->streamer);
if (self->nextInterval == NULL ||
    (minChrom != NULL && self->queryChrom != NULL &&
     (strcmp(self->queryChrom->name, minChrom) < 0)))
    asbbDoQuery(self, minChrom, minEnd);
if (self->nextInterval == NULL)
    return NULL;
char *chrom = sSelf->chrom ? sSelf->chrom : self->queryChrom->name;
// If current set of intervals is for minChrom, skip items to the left of minEnd:
if (minChrom != NULL && sameString(chrom, minChrom))
    {
    while (self->nextInterval && self->nextInterval->end < minEnd)
	self->nextInterval = self->nextInterval->next;
    // If no more items are left in intervalList, query again:
    if (self->nextInterval == NULL)
	asbbDoQuery(self, minChrom, minEnd);
    if (self->nextInterval == NULL)
	return NULL;
    }
int fieldCount = bigBedIntervalToRow(self->nextInterval, chrom,
				     self->startBuf, self->endBuf,
				     self->row, sSelf->numCols+1);
if (fieldCount != sSelf->numCols)
    errAbort("annoStreamBigBed %s: expected %d columns, got %d",
	     sSelf->name, sSelf->numCols, fieldCount);
self->nextInterval = self->nextInterval->next;
return self->row;
}

static struct annoRow *asbbNextRow(struct annoStreamer *vSelf, char *minChrom, uint minEnd,
				   struct lm *lm)
/* Return a single annoRow, or NULL if there are no more items. */
{
struct annoStreamBigBed *self = (struct annoStreamBigBed *)vSelf;
if (self->eof)
    return NULL;
char **row = nextRowUnfiltered(self, minChrom, minEnd);
if (row == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
boolean rightFail = FALSE;
while (annoFilterRowFails(vSelf->filters, row, vSelf->numCols, &rightFail))
    {
    if (rightFail)
	break;
    row = nextRowUnfiltered(self, minChrom, minEnd);
    if (row == NULL)
	return NULL;
    }
char *chrom = row[0];
uint chromStart = sqlUnsigned(row[1]);
uint chromEnd = sqlUnsigned(row[2]);
return annoRowFromStringArray(chrom, chromStart, chromEnd, rightFail, row, vSelf->numCols, lm);
}

static void asbbClose(struct annoStreamer **pVSelf)
/* Close bbi handle and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamBigBed *self = *(struct annoStreamBigBed **)pVSelf;
bigBedFileClose(&(self->bbi));
self->intervalList = NULL;
lmCleanup(&(self->intervalQueryLm));
freeMem(self->row);
freeMem(self->startBuf);
freeMem(self->endBuf);
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamBigBedNew(char *fileOrUrl, struct annoAssembly *aa, int maxItems)
/* Create an annoStreamer (subclass) object from a file or URL; if
 * maxItems is 0, all items from a query will be returned, otherwise
 * output is limited to maxItems. */
{
struct bbiFile *bbi = bigBedFileOpen(fileOrUrl);
struct asObject *asObj = bigBedAsOrDefault(bbi);
struct annoStreamBigBed *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj, fileOrUrl);
streamer->rowType = arWords;
streamer->setRegion = asbbSetRegion;
streamer->nextRow = asbbNextRow;
streamer->close = asbbClose;
self->bbi = bbi;
self->useMaxItems = (maxItems > 0);
self->maxItems = maxItems;
AllocArray(self->row, streamer->numCols + 1);
self->startBuf = needMem(32);
self->endBuf = needMem(32);
self->chromList = bbiChromList(bbi);
return (struct annoStreamer *)self;
}
