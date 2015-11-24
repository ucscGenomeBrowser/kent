/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoRow.h"

struct annoRow *annoRowFromStringArray(char *chrom, uint start, uint end, boolean rightJoinFail,
				       char **wordsIn, int numCols, struct lm *lm)
/* Allocate & return an annoRow (type arWords) with data cloned from wordsIn. */
{
struct annoRow *aRow;
lmAllocVar(lm, aRow);
aRow->chrom = lmCloneString(lm, chrom);
aRow->start = start;
aRow->end = end;
aRow->rightJoinFail = rightJoinFail;
aRow->data = lmCloneRow(lm, wordsIn, numCols);
return aRow;
}

struct annoRow *annoRowWigVecNew(char *chrom, uint start, uint end, boolean rightJoinFail,
                                 float *values, struct lm *lm)
/* Allocate & return an annoRow (type arWigVec), with cloned per-base values;
 * length of values is (end-start). */
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

struct annoRow *annoRowWigSingleNew(char *chrom, uint start, uint end, boolean rightJoinFail,
                                 double value, struct lm *lm)
/* Allocate & return an annoRow (type arWigSingle), which contains a single value for
 * all bases from start to end. */
{
struct annoRow *row;
lmAllocVar(lm, row);
row->chrom = lmCloneString(lm, chrom);
row->start = start;
row->end = end;
row->data = lmCloneMem(lm, &value, sizeof(value));
row->rightJoinFail = rightJoinFail;
return row;
}

struct annoRow *annoRowClone(struct annoRow *rowIn, enum annoRowType rowType, int numCols,
			     struct lm *lm)
/* Allocate & return a single annoRow cloned from rowIn.  If rowIn is NULL, return NULL.
 * If type is arWig*, numCols is ignored. */
{
if (rowIn == NULL)
    return NULL;
if (rowType == arWords)
    return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
				  rowIn->data, numCols, lm);
else if (rowType == arWigVec)
    return annoRowWigVecNew(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			 (float *)rowIn->data, lm);
else if (rowType == arWigSingle)
    return annoRowWigSingleNew(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
                               ((double *)rowIn->data)[0], lm);
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

