/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#include "annoRow.h"

struct annoRow *annoRowFromStringArray(char *chrom, uint chromStart, uint chromEnd,
				       char **wordsIn, int numCols)
/* Allocate & return an annoRow with words cloned from wordsIn. */
{
struct annoRow *aRow;
AllocVar(aRow);
aRow->chrom = cloneString(chrom);
aRow->chromStart = chromStart;
aRow->chromEnd = chromEnd;
char **words;
AllocArray(words, numCols);
int i;
for (i = 0;  i < numCols;  i++)
    words[i] = cloneString(wordsIn[i]);
aRow->words = words;
return aRow;
}

