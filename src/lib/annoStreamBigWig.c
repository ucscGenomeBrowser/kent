/* annoStreamBigWig -- subclass of annoStreamer for bigWig file or URL */

#include "annoStreamBigWig.h"
#include "bigWig.h"

char *annoRowBigWigAsText =
"table annoRowBigWig\n"
"\"autoSql description of a single annoRowBigWig value, for filtering\"\n"
"    (\n"
"    float value;  \"data value for this range\"\n"
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
    };


static void asbwSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free localmem from previous query if necessary. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamBigWig *self = (struct annoStreamBigWig *)vSelf;
self->nextInterval = self->intervalList = NULL;
lmCleanup(&(self->intervalQueryLm));
}

static void asbwDoQuery(struct annoStreamBigWig *self)
/* Store results of an interval query. [Would be nice to make a streaming version of this.] */
{
struct annoStreamer *streamer = &(self->streamer);
if (self->intervalQueryLm == NULL)
    self->intervalQueryLm = lmInit(0);
self->intervalList = bigWigIntervalQuery(self->bbi, streamer->chrom,
					 streamer->regionStart, streamer->regionEnd,
					 self->intervalQueryLm);
self->nextInterval = self->intervalList;
}

static struct annoRow *annoRowFromContigBbiIntervals(char *chrom,
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
	errAbort("annoRowFromContigBbiIntervals: overflowed baseCount (%s:%d-%d)",
		 chrom, startIv->start, endIv->end);
    }
return annoRowWigNew(chrom, startIv->start, endIv->end, rightJoinFail, vals, callerLm);
}

static struct annoRow *asbwNextRow(struct annoStreamer *vSelf, struct lm *callerLm)
/* Return a single annoRow, or NULL if there are no more items. */
{
struct annoStreamBigWig *self = (struct annoStreamBigWig *)vSelf;
if (self->intervalList == NULL)
    asbwDoQuery(self);
if (self->nextInterval == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing interval, or EOF.
boolean rightFail = FALSE;
struct bbiInterval *startIv = self->nextInterval;
while (annoFilterWigValueFails(vSelf->filters, self->nextInterval->val, &rightFail))
    {
    if (rightFail)
	break;
    startIv = self->nextInterval = self->nextInterval->next;
    if (self->nextInterval == NULL)
	return NULL;
    }
if (rightFail)
    return annoRowFromContigBbiIntervals(vSelf->chrom, startIv, startIv, rightFail, callerLm);
struct bbiInterval *endIv = startIv, *iv;
int maxCount = 16 * 1024, count;
for (iv = startIv->next, count = 0;  iv != NULL && count < maxCount;  iv = iv->next, count++)
    {
    // collect contiguous intervals; then make annoRow with vector.
    if (annoFilterWigValueFails(vSelf->filters, iv->val, &rightFail))
	break;
    if (iv->start == endIv->end)
	endIv = iv;
    else
	break;
    }
self->nextInterval = endIv->next;
return annoRowFromContigBbiIntervals(vSelf->chrom, startIv, endIv, rightFail, callerLm);
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

struct annoStreamer *annoStreamBigWigNew(char *fileOrUrl, struct annoAssembly *aa)
/* Create an annoStreamer (subclass) object from a file or URL. */
{
struct bbiFile *bbi = bigWigFileOpen(fileOrUrl);
struct asObject *asObj = asParseText(annoRowBigWigAsText);
struct annoStreamBigWig *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj);
streamer->rowType = arWig;
streamer->setRegion = asbwSetRegion;
streamer->nextRow = asbwNextRow;
streamer->close = asbwClose;
self->bbi = bbi;
return (struct annoStreamer *)self;
}
