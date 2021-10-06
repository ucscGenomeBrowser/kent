/* annoStreamWig -- subclass of annoStreamer for wiggle database tables */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamWig.h"
#include "annoStreamDb.h"
#include "jksql.h"
#include "wiggle.h"

char *annoRowWigAsText = 
"table annoRowWig\n"
"\"autoSql description of a single annoRowWig value, for filtering\"\n"
"    (\n"
"    string chrom;     \"Reference sequence chromosome or scaffold\"\n"
"    uint chromStart;  \"Start position in chromosome\"\n"
"    uint chromEnd;    \"End position in chromosome\"\n"
"    float value;      \"data value for this range\"\n"
"    )\n"
    ;

struct annoStreamWig
    {
    struct annoStreamer streamer;	// Parent class members & methods / external interface
    // Private members
    struct annoStreamer *wigStr;	// Internal streamer for .wig as in wiggle db tables
    struct udcFile *wibFH;		// wib file udcFile handle
    char *wibFile;			// name of wib file on which wibFH was opened
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
char *realWibFile = hReplaceGbdb(wibFile);
if (self->wibFile == NULL || !sameString(self->wibFile, realWibFile))
    {
    udcFileClose(&self->wibFH);
    freeMem(self->wibFile);
    self->wibFile = cloneString(realWibFile);
    self->wibFH = udcFileMayOpen(self->wibFile, NULL);
    if (self->wibFH == NULL)
        errAbort("annoStreamWig: udc can't open wibFile '%s'", self->wibFile);
    }
freeMem(realWibFile);
}

static void paranoidCheckSize(struct annoStreamWig *self, struct wiggle *wiggle)
/* paranoid, consider taking this out when code is stable: */
{
int bpLen = wiggle->chromEnd - wiggle->chromStart;
if (bpLen != (wiggle->span * wiggle->count))
    errAbort("annoStreamWig %s: length in bases (%u - %u = %d) != span*count (%u * %u = %u)",
	     self->streamer.name, wiggle->chromEnd, wiggle->chromStart, bpLen,
	     wiggle->span, wiggle->count, (wiggle->span * wiggle->count));
}

static void getFloatArray(struct annoStreamWig *self, struct wiggle *wiggle,
			  boolean *retRightFail, int *retValidCount, float *vector)
/* expand wiggle bytes & spans to per-bp floats; filter values here! */
{
udcSeek(self->wibFH, wiggle->offset);
UBYTE wigBuf[wiggle->count];
size_t expectedBytes = sizeof(wigBuf);
size_t bytesRead = udcRead(self->wibFH, wigBuf, expectedBytes);
if (bytesRead != expectedBytes)
    errnoAbort("annoStreamWig: failed to udcRead %llu bytes from %s (got %llu)\n",
	       (unsigned long long)expectedBytes, wiggle->file, (unsigned long long)bytesRead);
paranoidCheckSize(self, wiggle);
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

static struct annoRow *aswNextRow(struct annoStreamer *vSelf, char *minChrom, uint minEnd,
				  struct lm *callerLm)
/* Return an annoRow encoding the next chunk of wiggle data, or NULL if there are no more items. */
{
struct annoStreamWig *self = (struct annoStreamWig *)vSelf;
struct annoRow *rowOut = NULL;
boolean done = FALSE;
while (!done)
    {
    struct annoRow *wigRow = self->wigStr->nextRow(self->wigStr, minChrom, minEnd, callerLm);
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
	rowOut = annoRowWigVecNew(wigRow->chrom, wigRow->start, wigRow->end, rightFail, vector,
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
udcFileClose(&(self->wibFH));
freeMem(self->wibFile);
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamWigDbNew(char *db, char *table, struct annoAssembly *aa,
					int maxOutput)
/* Create an annoStreamer (subclass) object from a wiggle database table. */
{
struct annoStreamWig *self = NULL;
AllocVar(self);
self->wigStr = annoStreamDbNew(db, table, aa, maxOutput, NULL);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asParseText(annoRowWigAsText), self->wigStr->name);
streamer->rowType = arWigVec;
streamer->setRegion = aswSetRegion;
streamer->nextRow = aswNextRow;
streamer->close = aswClose;
return (struct annoStreamer *)self;
}
