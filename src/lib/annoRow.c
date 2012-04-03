/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#include "annoRow.h"
#include "annoStreamer.h"

struct annoRow *annoRowFromStringArray(char *chrom, uint start, uint end, boolean rightJoinFail,
				       char **wordsIn, int numCols)
/* Allocate & return an annoRow with words cloned from wordsIn. */
{
struct annoRow *aRow;
AllocVar(aRow);
aRow->chrom = cloneString(chrom);
aRow->start = start;
aRow->end = end;
aRow->rightJoinFail = rightJoinFail;
char **words;
AllocArray(words, numCols);
int i;
for (i = 0;  i < numCols;  i++)
    words[i] = cloneString(wordsIn[i]);
aRow->data = words;
return aRow;
}

struct annoRow *annoRowWigNew(char *chrom, uint start, uint end, boolean rightJoinFail,
			      float *values)
/* Allocate & return an annoRowWig, with clone of values; length of values is (end-start). */
{
struct annoRow *row;
AllocVar(row);
row->chrom = cloneString(chrom);
row->start = start;
row->end = end;
row->data = cloneMem(values, (end - start) * sizeof(values[0]));
row->rightJoinFail = rightJoinFail;
return row;
}

struct annoRow *annoRowClone(struct annoRow *rowIn, struct annoStreamer *source)
/* Allocate & return a single annoRow cloned from rowIn. */
{
if (rowIn == NULL)
    return NULL;
if (source->rowType == arWords || source->rowType == arVcf)
    return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
				  rowIn->data, source->numCols);
else if (source->rowType == arWig)
    return annoRowWigNew(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			 (float *)rowIn->data);
else
    errAbort("annoRowClone: unrecognized type %d", source->rowType);
return NULL;
}

static void annoRowWordsFree(struct annoRow **pRow, int numCols)
/* Free a single annoRow with type arWords. */
{
if (pRow == NULL)
    return;
struct annoRow *row = *pRow;
freeMem(row->chrom);
char **words = row->data;
int i;
for (i = 0;  i < numCols;  i++)
    freeMem(words[i]);
freeMem(row->data);
freez(pRow);
}

static void annoRowWigFree(struct annoRow **pRow)
/* Free a single annoRow with type arWig. */
{
if (pRow == NULL)
    return;
struct annoRow *row = *pRow;
freeMem(row->chrom);
freeMem(row->data);
freez(pRow);
}

void annoRowFree(struct annoRow **pRow, struct annoStreamer *source)
/* Free a single annoRow. */
{
if (pRow == NULL)
    return;
if (source->rowType == arWords || source->rowType == arVcf)
    annoRowWordsFree(pRow, source->numCols);
else if (source->rowType == arWig)
    annoRowWigFree(pRow);
else
    errAbort("annoRowFree: unrecognized type %d", source->rowType);
}

void annoRowFreeList(struct annoRow **pList, struct annoStreamer *source)
/* Free a list of annoRows. */
{
if (pList == NULL)
    return;
struct annoRow *row, *nextRow;
for (row = *pList;  row != NULL;  row = nextRow)
    {
    nextRow = row->next;
    annoRowFree(&row, source);
    }
}
