/* coordCols - parsing of file by specifying coordinate columns */
#include "common.h"
#include "coordCols.h"
#include "sqlNum.h"
#include "linefile.h"

static void invalidSpec(char *optName, char* spec)
/* generate an error, msg can be null */
{
errAbort("invalid coord column spec for %s: %s", optName, spec);
}

struct coordCols coordColsParseSpec(char *optName, char* spec)
/* parse coordinate specified in an option. Can be in the form
 *   - chromCol - chrom in this column followed by start and end.
 *   - chromCol,startCol,endCol - chrom, start, and end in specified
 *     columns.
 *   - chromCol,startCol,endCol,strandCol - chrom, start, end, and
 *     strand in specified columns.
 */
{
char buf[128];
int numWords;
char *words[4];
struct coordCols cols;
ZeroVar(&cols);
cols.strandCol = -1;

if (strlen(spec) > sizeof(buf)-1)
    invalidSpec(optName, spec);
strcpy(buf, spec);
numWords = chopString(buf, ",", words, ArraySize(words));
switch (numWords) {
case 1:
    cols.chromCol = sqlUnsigned(words[0]);
    cols.startCol = cols.chromCol + 1;
    cols.endCol = cols.chromCol + 2;
    break;
case 4:
    cols.strandCol = sqlUnsigned(words[3]);
case 3:
    cols.chromCol = sqlUnsigned(words[0]);
    cols.startCol = sqlUnsigned(words[1]);
    cols.endCol = sqlUnsigned(words[2]);
    break;
default:
    invalidSpec(optName, spec);
}
cols.minNumCols = cols.chromCol;
cols.minNumCols = max(cols.minNumCols, cols.startCol);
cols.minNumCols = max(cols.minNumCols, cols.endCol);
if (cols.strandCol >= 0)
    cols.minNumCols = max(cols.minNumCols, cols.strandCol);
cols.minNumCols++;

return cols;
}

struct coordColVals coordColParseRow(struct coordCols* cols, 
                                     struct lineFile* lf,
                                     char **row, int numCols)
/* parse coords from a row */
{
struct coordColVals colVals;
ZeroVar(&colVals);
lineFileExpectAtLeast(lf, cols->minNumCols, numCols);
colVals.chrom = row[cols->chromCol];
colVals.start = sqlUnsigned(row[cols->startCol]);
colVals.end = sqlUnsigned(row[cols->endCol]);
if (cols->strandCol >= 0)
    colVals.strand = row[cols->strandCol][0];
return colVals;
}

