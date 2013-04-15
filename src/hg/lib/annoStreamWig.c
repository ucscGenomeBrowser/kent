/* annoStreamWig -- subclass of annoStreamer for wiggle database tables */

#include "annoStreamWig.h"
#include "annoStreamDb.h"
#include "jksql.h"
#include "wiggle.h"

char *annoRowWigAsText = 
"table annoRowWig\n"
"\"autoSql description of a single annoRowWig value, for filtering\"\n"
"    (\n"
"    float  value;  \"data value for this range\"\n"
"    )\n"
    ;

struct annoStreamWig
    {
    struct annoStreamer streamer;	// Parent class members & methods / external interface
    // Private members
    struct annoStreamer *wigStr;	// Internal streamer for .wig as in wiggle db tables
    FILE *wibF;				// wib file handle
    char *wibFile;			// name of wib file on which wibF was opened
    };

static void aswSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free current sqlResult if there is one. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamWig *self = (struct annoStreamWig *)vSelf;
self->wigStr->setRegion(self->wigStr, chrom, regionStart, regionEnd);
}

static void checkWibFile(struct annoStreamWig *self, char *wibFile)
/* If self doesn't have a .wib file name and handle open, or if the new wibFile is
 * not the same as the old one, update self to use new wibFile. */
{
if (self->wibFile == NULL || !sameString(self->wibFile, wibFile))
    {
    carefulClose(&(self->wibF));
    freeMem(self->wibFile);
    self->wibFile = cloneString(wibFile);
    self->wibF = mustOpen(self->wibFile, "r");
    }
}

static void paranoidCheckSize(struct wiggle *wiggle)
/* paranoid, consider taking this out when code is stable: */
{
int bpLen = wiggle->chromEnd - wiggle->chromStart;
if (bpLen != (wiggle->span * wiggle->count))
    errAbort("annoStreamWig: length in bases (%u - %u = %d) != span*count (%u * %u = %u)",
	     wiggle->chromEnd, wiggle->chromStart, bpLen,
	     wiggle->span, wiggle->count, (wiggle->span * wiggle->count));
}

static void getFloatArray(struct annoStreamWig *self, struct wiggle *wiggle,
			  boolean *retRightFail, int *retValidCount, float *vector)
/* expand wiggle bytes & spans to per-bp floats; filter values here! */
{
fseek(self->wibF, wiggle->offset, SEEK_SET);
UBYTE wigBuf[wiggle->count];
size_t bytesRead = fread(wigBuf, 1, wiggle->count, self->wibF);
if (bytesRead != wiggle->count)
    errnoAbort("annoStreamWig: failed to read %u bytes from %s (got %llu)\n",
	       wiggle->count, wiggle->file, (unsigned long long)bytesRead);
paranoidCheckSize(wiggle);
int i, j, validCount = 0;
for (i = 0;  i < wiggle->count;  i++)
    {
    float value;
    if (wigBuf[i] == WIG_NO_DATA)
	value = NAN;
    else
	{
	value = BIN_TO_VALUE(wigBuf[i], wiggle->lowerLimit, wiggle->dataRange);
	if (annoFilterWigValueFails(self->streamer.filters, value, retRightFail))
	    value = NAN;
	else
	    validCount++;
	}
    int bpOffset = i * wiggle->span;
    for (j = 0;  j < wiggle->span;  j++)
	vector[bpOffset + j] = value;
    }
if (retValidCount != NULL)
    *retValidCount = validCount;
}

static struct annoRow *aswNextRow(struct annoStreamer *vSelf, struct lm *callerLm)
/* Return an annoRow encoding the next chunk of wiggle data, or NULL if there are no more items. */
{
struct annoStreamWig *self = (struct annoStreamWig *)vSelf;
struct annoRow *rowOut = NULL;
boolean done = FALSE;
while (!done)
    {
    struct annoRow *wigRow = self->wigStr->nextRow(self->wigStr, callerLm);
    if (wigRow == NULL)
	return NULL;
    struct wiggle wiggle;
    wiggleStaticLoad((char **)wigRow->data, &wiggle);
    checkWibFile(self, wiggle.file);
    // translate wigRow + bytes to float vector
    boolean rightFail = FALSE;
    int validCount = 0;
    int bpLen = wiggle.chromEnd - wiggle.chromStart;
    float vector[bpLen];
    getFloatArray(self, &wiggle, &rightFail, &validCount, vector);
    if (rightFail || validCount > 0)
	{
	rowOut = annoRowWigNew(wigRow->chrom, wigRow->start, wigRow->end, rightFail, vector,
			       callerLm);
	done = TRUE;
	}
    }
return rowOut;
}

static void aswClose(struct annoStreamer **pVSelf)
/* Free wiggleDataStream and self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamWig *self = *(struct annoStreamWig **)pVSelf;
carefulClose(&(self->wibF));
freeMem(self->wibFile);
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamWigDbNew(char *db, char *table, struct annoAssembly *aa,
					int maxOutput)
/* Create an annoStreamer (subclass) object from a wiggle database table. */
{
struct annoStreamWig *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asParseText(annoRowWigAsText));
streamer->rowType = arWig;
streamer->setRegion = aswSetRegion;
streamer->nextRow = aswNextRow;
streamer->close = aswClose;
self->wigStr = annoStreamDbNew(db, table, aa, asParseText(wiggleAsText));
return (struct annoStreamer *)self;
}
