/* coordCols - parsing of file by specifying coordinate columns */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "coordCols.h"
#include "rowReader.h"
#include "sqlNum.h"


static void invalidSpec(char *optName, char* spec)
/* generate an error, msg can be null */
{
errAbort("invalid coord column spec for %s: %s", optName, spec);
}

static int colIdxOrOmitted(char **words, int numWords, int iWord)
/* return the column index or -1 if omitted by empty or the end */
{
if ((iWord >= numWords) || (strlen(words[iWord]) == 0))
    return -1;
else
    return sqlUnsigned(words[iWord]);
}

struct coordCols coordColsParseSpec(char *optName, char* spec)
/* parse coordinate specified in an option. Can be in the form
 *   - chromCol - chrom in this column followed by start and end.
 *   - chromCol,startCol,endCol,strandCol,name - chrom, start, end, and
 *     strand in specified columns.  With unneeded columns empty or dropped
 *     from the end.
 */
{
char buf[128];
int numWords;
char *words[6];
struct coordCols cols;
ZeroVar(&cols);
cols.strandCol = -1;

if (strlen(spec) > sizeof(buf)-1)
    invalidSpec(optName, spec);
safecpy(buf, sizeof(buf), spec);
numWords = chopByChar(buf, ',', words, ArraySize(words));
if (numWords == 1)
    {
    cols.chromCol = sqlUnsigned(words[0]);
    cols.startCol = cols.chromCol + 1;
    cols.endCol = cols.chromCol + 2;
    }
else if (numWords <= 5)
    {
    cols.chromCol = sqlUnsigned(words[0]);
    cols.startCol = sqlUnsigned(words[1]);
    cols.endCol = sqlUnsigned(words[2]);
    cols.strandCol = colIdxOrOmitted(words, numWords, 3);
    cols.nameCol = colIdxOrOmitted(words, numWords, 4);
    }
else
    invalidSpec(optName, spec);
// find minimum number of columns
cols.minNumCols = cols.chromCol+1;
cols.minNumCols = max(cols.minNumCols, cols.startCol+1);
cols.minNumCols = max(cols.minNumCols, cols.endCol+1);
if (cols.strandCol >= 0)
    cols.minNumCols = max(cols.minNumCols, cols.strandCol+1);
if (cols.nameCol >= 0)
    cols.minNumCols = max(cols.minNumCols, cols.nameCol+1);
return cols;
}

struct coordColVals coordColParseRow(struct coordCols* cols,
                                     struct rowReader *rr)
/* parse coords from a row, call coordColsRelease when done with result */
{
struct coordColVals colVals;
ZeroVar(&colVals);
rowReaderExpectAtLeast(rr, cols->minNumCols);
colVals.chrom = rr->row[cols->chromCol];
colVals.start = sqlUnsigned(rr->row[cols->startCol]);
colVals.end = sqlUnsigned(rr->row[cols->endCol]);
if (cols->strandCol >= 0)
    colVals.strand = rr->row[cols->strandCol][0];
if (cols->nameCol >= 0)
    colVals.name = cloneString(rr->row[cols->nameCol]);
return colVals;
}

