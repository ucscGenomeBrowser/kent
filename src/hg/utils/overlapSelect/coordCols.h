/* coordCols - parsing of file by specifying coordinate columns */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef COORD_COLS_H
#define COORD_COLS_H
struct rowReader;

struct coordCols
/* coordinate columns to parse from file */
{
    int chromCol;
    int startCol;
    int endCol;
    int strandCol;     /* -1 if unspecified */
    int nameCol;       /* -1 if unspecified */
    int minNumCols;    /* min number of cols */
};

struct coordColVals 
/* values parsed from a row */
{
    char *chrom;
    int start;
    int end;
    char strand;
    char *name;   // dynamic
};

struct coordCols coordColsParseSpec(char *optName, char* optValue);
/* parse coordinate specified in an option. Can be in the form
 *   - chromCol - chrom in this column followed by start and end.
 *   - chromCol,startCol,endCol,strandCol,name - chrom, start, end, and
 *     strand in specified columns.  With unneeded columns empty or dropped
 *     from the end.
 */

struct coordColVals coordColParseRow(struct coordCols* cols,
                                     struct rowReader *rr);
/* parse coords from a row, call coordColsRelease when done with result */


INLINE void coordColsValsRelease(struct coordColVals *colVals)
/* free any dynamic memory in colVals.  ColVals is automatic, so is not
 * freed */
{
freez(&colVals->name);
}

#endif
