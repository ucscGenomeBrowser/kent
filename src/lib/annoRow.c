/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#include "annoRow.h"

struct annoRow *annoRowFromStringArray(char *chrom, uint start, uint end, boolean rightJoinFail,
				       char **wordsIn, int numCols, struct lm *lm)
/* Allocate & return an annoRow with words cloned from wordsIn. */
{
struct annoRow *aRow;
lmAllocVar(lm, aRow);
aRow->chrom = lmCloneString(lm, chrom);
aRow->start = start;
aRow->end = end;
aRow->rightJoinFail = rightJoinFail;
char **words;
lmAllocArray(lm, words, numCols);
int i;
for (i = 0;  i < numCols;  i++)
    words[i] = lmCloneString(lm, wordsIn[i]);
aRow->data = words;
return aRow;
}

struct annoRow *annoRowWigNew(char *chrom, uint start, uint end, boolean rightJoinFail,
			      float *values, struct lm *lm)
/* Allocate & return an annoRowWig, with clone of values; length of values is (end-start). */
{
struct annoRow *row;
lmAllocVar(lm, row);
row->chrom = lmCloneString(lm, chrom);
row->start = start;
row->end = end;
row->data = lmCloneMem(lm, values, (end - start) * sizeof(values[0]));
row->rightJoinFail = rightJoinFail;
return row;
}

struct annoRow *annoRowClone(struct annoRow *rowIn, enum annoRowType rowType, int numCols,
			     struct lm *lm)
/* Allocate & return a single annoRow cloned from rowIn.  If rowIn is NULL, return NULL.
 * If type is arWig, numCols is ignored. */
{
if (rowIn == NULL)
    return NULL;
if (rowType == arWords || rowType == arVcf)
    return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
				  rowIn->data, numCols, lm);
else if (rowType == arWig)
    return annoRowWigNew(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			 (float *)rowIn->data, lm);
else
    errAbort("annoRowClone: unrecognized type %d", rowType);
return NULL;
}

int annoRowCmp(const void *va, const void *vb)
/* Compare two annoRows' {chrom, start, end}. */
{
struct annoRow *rowA = *((struct annoRow **)va);
struct annoRow *rowB = *((struct annoRow **)vb);
int dif = strcmp(rowA->chrom, rowB->chrom);
if (dif == 0)
    {
    dif = rowA->start - rowB->start;
    if (dif == 0)
	dif = rowA->end - rowB->end;
    }
return dif;
}

