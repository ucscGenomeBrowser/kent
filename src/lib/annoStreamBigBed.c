/* annoStreamBigBed -- subclass of annoStreamer for bigBed file or URL */

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
    struct bbiChromInfo *queryChrom;	// most recently queried chrom (or NULL) for whole-genome
    int maxItems;			// max items returned from bigBedIntervalQuery
    char **row;				// storage for results of bigBedIntervalToRow
    char *startBuf;			// storage for stringified start from bigBedIntervalToRow
    char *endBuf;			// storage for stringified end from bigBedIntervalToRow
    };

static void asbbSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free localmem from previous query if necessary. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamBigBed *self = (struct annoStreamBigBed *)vSelf;
self->nextInterval = self->intervalList = NULL;
lmCleanup(&(self->intervalQueryLm));
}

static void asbbDoQuery(struct annoStreamBigBed *self)
/* Store results of an interval query. [Would be nice to make a streaming version of this.] */
{
struct annoStreamer *sSelf = &(self->streamer);
if (self->intervalQueryLm == NULL)
    self->intervalQueryLm = lmInit(0);
if (sSelf->chrom != NULL)
    {
    self->intervalList = bigBedIntervalQuery(self->bbi, sSelf->chrom,
					     sSelf->regionStart, sSelf->regionEnd,
					     self->maxItems, self->intervalQueryLm);
    }
else
    {
    // Genome-wide query: break it into chrom-by-chrom queries.
    if (self->queryChrom == NULL)
	self->queryChrom = self->chromList;
    else
	self->queryChrom = self->queryChrom->next;
    if (self->queryChrom == NULL)
	{
	self->chromList = NULL; // EOF, don't start over!
	self->intervalList = NULL;
	}
    else
	{
	char *chrom = self->queryChrom->name;
	uint end = self->queryChrom->size;
	self->intervalList = bigBedIntervalQuery(self->bbi, chrom, 0, end,
						 self->maxItems, self->intervalQueryLm);
	}
    }
self->nextInterval = self->intervalList;
}

static char **nextRowUnfiltered(struct annoStreamBigBed *self)
/* Convert the next available interval into a row of words, or return NULL. */
{
struct annoStreamer *sSelf = &(self->streamer);
if (self->nextInterval == NULL)
    return NULL;
char *chrom = sSelf->chrom ? sSelf->chrom : self->queryChrom->name;
int fieldCount = bigBedIntervalToRow(self->nextInterval, chrom,
				     self->startBuf, self->endBuf,
				     self->row, sSelf->numCols+1);
if (fieldCount != sSelf->numCols)
    errAbort("annoStreamBigBed %s: expected %d columns, got %d",
	     sSelf->name, sSelf->numCols, fieldCount);
self->nextInterval = self->nextInterval->next;
return self->row;
}

static struct annoRow *asbbNextRow(struct annoStreamer *vSelf, struct lm *lm)
/* Return a single annoRow, or NULL if there are no more items. */
{
struct annoStreamBigBed *self = (struct annoStreamBigBed *)vSelf;
if (self->intervalList == NULL)
    asbbDoQuery(self);
char **row = nextRowUnfiltered(self);
if (row == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
boolean rightFail = FALSE;
while (annoFilterRowFails(vSelf->filters, row, vSelf->numCols, &rightFail))
    {
    if (rightFail)
	break;
    row = nextRowUnfiltered(self);
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
 * each query is limited to maxItems. */
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
self->maxItems = maxItems;
AllocArray(self->row, streamer->numCols + 1);
self->startBuf = needMem(32);
self->endBuf = needMem(32);
self->chromList = bbiChromList(bbi);
return (struct annoStreamer *)self;
}
