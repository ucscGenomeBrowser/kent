/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#include "annoRow.h"

struct annoRow *annoRowFromStringArray(char *chrom, uint chromStart, uint chromEnd,
				       char **rowIn, int numCols)
/* Allocate & return an annoRow with data = new char ** row, with elements copied from rowIn. */
{
struct annoRow *aRow;
AllocVar(aRow);
aRow->chrom = cloneString(chrom);
aRow->chromStart = chromStart;
aRow->chromEnd = chromEnd;
char **rowOut;
AllocArray(rowOut, numCols);
int i;
for (i = 0;  i < numCols;  i++)
    rowOut[i] = cloneString(rowIn[i]);
aRow->data = rowOut;
return aRow;
}

