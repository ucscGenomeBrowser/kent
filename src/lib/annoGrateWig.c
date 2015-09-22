/* annoGrateWig -- subclass of annoGrator for bigWig or wiggle data */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoGrateWig.h"
#include "annoStreamBigWig.h"

struct annoGrateWig
{
    struct annoGrator grator;		// external annoGrator interface
    struct annoGrator *mySource;	// internal annoGrator of wigDb rows
    struct lm *lm;			// localmem for rows from mySource
    int lmRowCount;			// counter for periodic cleanup
    enum annoGrateWigMode mode;         // output: either one annoRow with average value in range
                                        // or one or more annoRows excluding bases with NANs
};

static struct annoRow *normalizeRows(const struct annoRow *rowsIn,
                                     uint regionStart, uint regionEnd, struct lm *callerLm)
/* This takes a series of wiggle chunks coming from .wig/database rows and makes it into
 * zero or more tidy little NAN-less annoRows.  Trim rowIn to the bounds of
 * region, trim NANs from beginning and break into multiple rows where there
 * are NANs in the middle.  If the rowIn is contiguous with the row at the
 * head of outList, expand that row to include rowIn's data. */
{
struct annoRow *rowOutList = NULL;
const struct annoRow *rowIn;
for (rowIn = rowsIn;  rowIn != NULL;  rowIn = rowIn->next)
    {
    uint start = max(rowIn->start, regionStart);
    uint end = min(rowIn->end, regionEnd);
    float *vector = rowIn->data;
    while (end > start)
        {
        uint offset = start - rowIn->start;
        if (isnan(vector[offset]))
            start++;
        else
            {
            // If there is a NAN before end, that's the end of this row:
            uint thisEnd = start;
            while (thisEnd < end && ! isnan(vector[thisEnd - rowIn->start]))
                thisEnd++;
            struct annoRow *headRow = rowOutList;
            if (headRow == NULL || rowIn->start > headRow->end)
                {
                // allocate a new row
                struct annoRow *rowOut = annoRowWigVecNew(rowIn->chrom, start, thisEnd, FALSE,
                                                          vector + offset, callerLm);
                slAddHead(&rowOutList, rowOut);
                }
            else
                {
                // glom new data onto headRow
                if (thisEnd < headRow->end)
                    errAbort("Expected thisEnd (%u) to be greater than or equal to "
                             "headRow->end (%u)",
                             thisEnd, headRow->end);
                uint oldEnd = headRow->end;
                uint oldLen = oldEnd - headRow->start;
                uint newLen = thisEnd - headRow->start;
                //#*** TODO: precompute how many bytes we will need, so we don't have to realloc!!
                headRow->data = lmAllocMoreMem(callerLm, headRow->data, oldLen*sizeof(vector[0]),
                                               newLen*sizeof(vector[0]));
                headRow->end = thisEnd;
                float *newData = (float *)rowIn->data + (oldEnd - rowIn->start);
                float *newSpace = (float *)headRow->data + oldLen;
                CopyArray(newData, newSpace, (thisEnd - oldEnd));
                }
            start = thisEnd;
            }
        }
    }
slReverse(&rowOutList);
return rowOutList;
}

static struct annoRow *averageRows(struct annoRow *rowList, uint regionStart, uint regionEnd,
                                   struct lm *callerLm)
/* Return an annoRow with the average value of floats across all row->data (within region),
 * ignoring missing data and NANs.  Return NULL if rowList is NULL or if all bases overlapping
 * the region are NAN (unlikely, but could happen). */
{
if (rowList == NULL)
    return NULL;
int count = 0;
double sum = 0.0;
uint start = max(rowList->start, regionStart);
uint end = start;
struct annoRow *row;
for (row = rowList;  row != NULL;  row = row->next)
    {
    uint rowStart = max(row->start, regionStart);
    uint rowEnd = min(row->end, regionEnd);
    if (rowEnd > rowStart)
        {
        float *vector = row->data;
        int iStart = rowStart - row->start;
        int iEnd = rowEnd - row->start;
        int i;
        for (i = iStart;  i < iEnd;  i++)
            {
            // skip NAN values so they don't convert sum to a NAN:
            if (! isnan(vector[i]))
                {
                sum += vector[i];
                count++;
                }
            }
        end = rowEnd;
        }
    }
if (count > 0)
    {
    double average = (sum / (double)count);
    return annoRowWigSingleNew(rowList->chrom, start, end, FALSE, average, callerLm);
    }
return NULL;
}

static struct annoRow *agwIntegrate(struct annoGrator *gSelf, struct annoStreamRows *primaryData,
				    boolean *retRJFilterFailed, struct lm *callerLm)
/* Return wig annoRows that overlap primaryRow position, with NANs weeded out. */
{
struct annoGrateWig *self = (struct annoGrateWig *)gSelf;
// Cleanup internal lm every so often:
if (self->lmRowCount >= 4096)
    {
    lmCleanup(&(self->lm));
    self->lmRowCount = 0;
    }
if (self->lm == NULL)
    self->lm = lmInit(0);
struct annoRow *rowsIn = annoGratorIntegrate(self->mySource, primaryData, retRJFilterFailed,
					     self->lm);
self->lmRowCount += slCount(rowsIn);
if (retRJFilterFailed && *retRJFilterFailed)
    return NULL;
struct annoRow *primaryRow = primaryData->rowList;
struct annoStreamer *sSelf = (struct annoStreamer *)gSelf;
uint regionStart, regionEnd;
if (isNotEmpty(sSelf->chrom))
    {
    regionStart = max(primaryRow->start, sSelf->regionStart);
    regionEnd = min(primaryRow->end, sSelf->regionEnd);
    }
else
    {
    regionStart = primaryRow->start;
    regionEnd = primaryRow->end;
    }
if (self->mode == agwmAverage)
    return averageRows(rowsIn, regionStart, regionEnd, callerLm);
else if (self->mode == agwmPerBase)
    return normalizeRows(rowsIn, regionStart, regionEnd, callerLm);
else
    errAbort("agwIntegrate: invalid self->mode %d", self->mode);
return NULL;
}

static void appendAverageToValueLabel(struct asObject *asObj)
/* When we are averaging wiggle values, tweak the "value" column label to remind the user. */
{
struct asColumn *col;
for (col = asObj->columnList;  col != NULL;  col = col->next)
    {
    if (sameString(col->name, "value"))
        {
        freez(&col->name);
        col->name = cloneString("valueAverage");
        }
    }
}

struct annoGrator *annoGrateWigNew(struct annoStreamer *wigSource, enum annoGrateWigMode mode)
/* Create an annoGrator subclass for source with wiggle or bigWig data.
 * If mode is agwmAverage, then this produces at most one annoRow per primary item
 * with type arWigSingle, containing the average of all overlapping non-NAN values.
 * If mode is agwmPerBase, then this produces a list of annoRows split by NANs, with type
 * arWigVec (vector of per-base values). */
{
if (wigSource->rowType != arWigVec)
    errAbort("annoGrateWigNew: expected source->rowType arWigVec (%d), got %d from source %s",
	     arWigVec, wigSource->rowType, wigSource->name);
struct annoGrateWig *self;
AllocVar(self);
struct annoGrator *gSelf = (struct annoGrator *)self;
annoGratorInit(gSelf, wigSource);
gSelf->integrate = agwIntegrate;
self->mySource = annoGratorNew(wigSource);
self->mode = mode;
struct annoStreamer *sSelf = (struct annoStreamer *)gSelf;
if (mode == agwmAverage)
    {
    sSelf->rowType = arWigSingle;
    appendAverageToValueLabel(sSelf->asObj);
    }
else if (mode == agwmPerBase)
    sSelf->rowType = arWigVec;
else
    errAbort("annoGrateWigNew: unrecognized enum annoGrateWigMode %d", mode);
return gSelf;
}

struct annoGrator *annoGrateBigWigNew(char *fileOrUrl, struct annoAssembly *aa,
                                      enum annoGrateWigMode mode)
/* Create an annoGrator subclass for bigWig file or URL.  See above description of mode. */
{
struct annoStreamer *bigWigSource = annoStreamBigWigNew(fileOrUrl, aa);
return annoGrateWigNew(bigWigSource, mode);
}
