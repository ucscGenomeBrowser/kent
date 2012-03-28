/* annoGrateWig -- subclass of annoGrator for wiggle data */

#include "annoGrateWig.h"
#include "annoStreamWig.h"
#include "sqlNum.h"
#include "wiggle.h"

static void tidyUp(struct annoRow *rowIn, struct annoRow **pOutList,
		   uint primaryStart, uint primaryEnd)
/* This takes a wiggle chunk coming from a .wig/database row and makes it into
 * zero or more tidy little NAN-less annoRows.  Trim rowIn to the bounds of
 * primary, trim NANs from beginning and break into multiple rows where there
 * are NANs in the middle.  If the rowIn is contiguous with the row at the
 * head of outList, expand that row to include rowIn's data. */
{
uint start = max(rowIn->start, primaryStart);
uint end = min(rowIn->end, primaryEnd);
float *vector = rowIn->data;
while (end > start)
    {
    uint offset = start - rowIn->start;
    if (vector[offset] == NAN)
	start++;
    else
	{
	// If there is a NAN before end, that's the end of this row:
	uint thisEnd = start;
	while (thisEnd < end && vector[thisEnd - rowIn->start] != NAN)
	    thisEnd++;
	struct annoRow *headRow = *pOutList;
	if (headRow == NULL || rowIn->start > headRow->end)
	    {
	    // allocate a new row
	    struct annoRow *rowOut = annoRowWigNew(rowIn->chrom, start, thisEnd, FALSE,
						   vector + offset);
	    slAddHead(pOutList, rowOut);
	    }
	else
	    {
	    // glom new data onto headRow
	    assert(thisEnd > headRow->end);
	    uint oldEnd = headRow->end;
	    uint oldLen = oldEnd - headRow->start;
	    uint newLen = thisEnd - headRow->start;
	    headRow->data = needMoreMem(headRow->data, oldLen, newLen);
	    headRow->end = thisEnd;
	    float *newData = (float *)rowIn->data + (oldEnd - rowIn->start);
	    float *newSpace = (float *)headRow->data + (oldEnd - headRow->start);
	    CopyArray(newData, newSpace, (thisEnd - oldEnd));
	    }
	start = thisEnd;
	}
    }
}

static struct annoRow *agwdIntegrate(struct annoGrator *self, struct annoRow *primaryRow,
				     boolean *retRJFilterFailed)
/* Return wig annoRows that overlap primaryRow position, with NANs weeded out. */
{
struct annoRow *rowsIn = annoGratorIntegrate(self, primaryRow, retRJFilterFailed);
if (retRJFilterFailed && *retRJFilterFailed)
    return NULL;
struct annoRow *rowIn, *rowOutList = NULL;;
for (rowIn = rowsIn;  rowIn != NULL;  rowIn = rowIn->next)
    tidyUp(rowIn, &rowOutList, primaryRow->start, primaryRow->end);
slReverse(&rowOutList);
annoRowFreeList(&rowsIn, -1);
return rowOutList;
}

struct annoGrator *annoGrateWigDbNew(char *db, char *table, int maxOutput)
/* Create an annoGrator subclass for wiggle data from db.table (and the file it points to). */
{
struct annoStreamer *wigSource = annoStreamWigDbNew(db, table, maxOutput);
struct annoGrator *self = annoGratorNew(wigSource);
self->integrate = agwdIntegrate;
return self;
}
