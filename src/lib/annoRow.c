/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#include "annoRow.h"

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
aRow->words = words;
return aRow;
}

struct annoRow *annoRowClone(struct annoRow *rowIn, int numCols)
/* Allocate & return a single annoRow cloned from rowIn. */
{
return annoRowFromStringArray(rowIn->chrom, rowIn->start, rowIn->end, rowIn->rightJoinFail,
			      rowIn->words, numCols);
}
