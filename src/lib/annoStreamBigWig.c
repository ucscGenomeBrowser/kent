/* annoStreamBigWig -- subclass of annoStreamer for bigWig file or URL */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamBigWig.h"
#include "bigWig.h"

char *annoRowBigWigAsText =
"table annoRowBigWig\n"
"\"autoSql description of a single annoRowBigWig value, for filtering\"\n"
"    (\n"
"    string chrom;     \"Reference sequence chromosome or scaffold\"\n"
"    uint chromStart;  \"Start position in chromosome\"\n"
"    uint chromEnd;    \"End position in chromosome\"\n"
"    float value;      \"data value for this range\"\n"
"    )\n"
    ;

struct annoStreamBigWig
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    struct bbiFile *bbi;		// bbi handle for bigBed file/URL.
    struct lm *intervalQueryLm;		// localmem object for bigWigIntervalQuery
    struct bbiInterval *intervalList;	// results of bigWigIntervalQuery
    struct bbiInterval *nextInterval;	// next result to be translated into row
    struct bbiChromInfo *chromList;	// list of chromosomes for which bbi actually has data
    struct bbiChromInfo *queryChrom;	// most recently queried chrom for whole-genome (or NULL)
    boolean eof;			// TRUE when we are done
    };


static void asbwSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free localmem from previous query if necessary. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamBigWig *self = (struct annoStreamBigWig *)vSelf;
self->nextInterval = self->intervalList = NULL;
self->queryChrom = NULL;
self->eof = FALSE;
lmCleanup(&(self->intervalQueryLm));
}

static void asbwDoQuery(struct annoStreamBigWig *self, char *minChrom, uint minEnd)
/* Store results of an interval query. [Would be nice to make a streaming version of this.] */
{
struct annoStreamer *sSelf = &(self->streamer);
if (self->intervalQueryLm == NULL)
    self->intervalQueryLm = lmInit(0);
if (sSelf->chrom != NULL)
    {
    uint start = sSelf->regionStart;
    if (minChrom)
	{
	if (differentString(minChrom, sSelf->chrom))
	    errAbort("annoStreamBigWig %s: nextRow minChrom='%s' but region chrom='%s'",
		     sSelf->name, minChrom, sSelf->chrom);
	if (start < minEnd)
	    start = minEnd;
	}
    self->intervalList = bigWigIntervalQuery(self->bbi, sSelf->chrom, start, sSelf->regionEnd,
					     self->intervalQueryLm);
    // If there are no intervals in the query region, we're done.
    if (self->intervalList == NULL)
        self->eof = TRUE;
    }
else
    {
    // Genome-wide query: break it into chrom-by-chrom queries.
    if (self->queryChrom == NULL)
	self->queryChrom = self->chromList;
    else
	self->queryChrom = self->queryChrom->next;
    if (minChrom != NULL)
	{
	// Skip chroms that precede minChrom
	while (self->queryChrom != NULL && strcmp(self->queryChrom->name, minChrom) < 0)
	    self->queryChrom = self->queryChrom->next;
	}
    if (self->queryChrom == NULL)
	{
	self->eof = TRUE;
	self->intervalList = NULL;
	}
    else
	{
	char *chrom = self->queryChrom->name;
	int start = 0;
	if (minChrom != NULL && sameString(chrom, minChrom))
	    start = minEnd;
	uint end = self->queryChrom->size;
	self->intervalList = bigWigIntervalQuery(self->bbi, chrom, start, end,
						 self->intervalQueryLm);
	}
    }
self->nextInterval = self->intervalList;
}

static struct annoRow *annoRowFromContigBbiIntervals(char *name, char *chrom,
				struct bbiInterval *startIv, struct bbiInterval *endIv,
				boolean rightJoinFail, struct lm *callerLm)
/* Given a range of non-NULL contiguous bbiIntervals (i.e. no gaps between intervals),
 * translate into annoRow with annoVector as data. */
{
float *vals;
int baseCount = endIv->end - startIv->start;
AllocArray(vals, baseCount);
int vecOff = 0;
struct bbiInterval *iv;
for (iv = startIv;  iv != endIv->next;  iv = iv->next)
    {
    int i;
    for (i = 0;  i < (iv->end - iv->start);  i++)
	vals[vecOff++] = iv->val;
    if (vecOff > baseCount)
	errAbort("annoStreamBigWig %s: overflowed baseCount (%s:%d-%d)",
		 name, chrom, startIv->start, endIv->end);
    }
return annoRowWigVecNew(chrom, startIv->start, endIv->end, rightJoinFail, vals, callerLm);
}

static struct annoRow *asbwNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				   struct lm *callerLm)
/* Return a single annoRow, or NULL if there are no more items. */
{
struct annoStreamBigWig *self = (struct annoStreamBigWig *)sSelf;
if (self->eof)
    return NULL;
else if (self->nextInterval == NULL)
    {
    asbwDoQuery(self, minChrom, minEnd);
    if (self->eof)
        return NULL;
    }
// Skip past any left-join failures until we get a right-join failure, a passing interval, or EOF.
boolean rightFail = FALSE;
struct bbiInterval *startIv = self->nextInterval;
while (annoFilterWigValueFails(sSelf->filters, self->nextInterval->val, &rightFail))
    {
    if (rightFail)
	break;
    startIv = self->nextInterval = self->nextInterval->next;
    if (self->nextInterval == NULL)
	return NULL;
    }
char *chrom = sSelf->chrom ? sSelf->chrom : self->queryChrom->name;
if (rightFail)
    return annoRowFromContigBbiIntervals(sSelf->name, chrom, startIv, startIv, rightFail,
					 callerLm);
// Collect up to maxCount contiguous intervals; then make annoRow with vector.
struct bbiInterval *endIv = startIv, *iv;
int maxCount = 16 * 1024, count;
for (iv = startIv->next, count = 0;  iv != NULL && count < maxCount;  iv = iv->next, count++)
    {
    if (annoFilterWigValueFails(sSelf->filters, iv->val, &rightFail))
	break;
    if (iv->start == endIv->end)
	endIv = iv;
    else
	break;
    }
// If there's a query region and we have reached the end of its intervals, we're done.
if (sSelf->chrom != NULL && endIv->next == NULL)
    self->eof = TRUE;
self->nextInterval = endIv->next;
return annoRowFromContigBbiIntervals(sSelf->name, chrom, startIv, endIv, rightFail, callerLm);
}

static void asbwClose(struct annoStreamer **pVSelf)
/* Close bbi handle and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamBigWig *self = *(struct annoStreamBigWig **)pVSelf;
bigWigFileClose(&(self->bbi));
self->intervalList = NULL;
lmCleanup(&(self->intervalQueryLm));
annoStreamerFree(pVSelf);
}

struct asObject *annoStreamBigWigAsObject()
/* Return an asObj that describes annoRow contents for wiggle (just float value). */
{
return asParseText(annoRowBigWigAsText);
}

struct annoStreamer *annoStreamBigWigNew(char *fileOrUrl, struct annoAssembly *aa)
/* Create an annoStreamer (subclass) object from a file or URL. */
{
struct bbiFile *bbi = bigWigFileOpen(fileOrUrl);
struct asObject *asObj = annoStreamBigWigAsObject();
struct annoStreamBigWig *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj, fileOrUrl);
//#*** Would be more memory-efficient to do arWigSingle for bedGraphs.
//#*** annoGrateWig would need to be updated to handle incoming arWigSingle.
streamer->rowType = arWigVec;
streamer->setRegion = asbwSetRegion;
streamer->nextRow = asbwNextRow;
streamer->close = asbwClose;
self->chromList = bbiChromList(bbi);
self->bbi = bbi;
return (struct annoStreamer *)self;
}
