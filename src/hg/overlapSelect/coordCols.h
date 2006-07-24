/* coordCols - parsing of file by specifying coordinate columns */
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
    int minNumCols;    /* min number of cols */
};

struct coordColVals 
/* values parsed from a row */
{
    char *chrom;
    int start;
    int end;
    char strand;
};

struct coordCols coordColsParseSpec(char *optName, char* optValue);
/* parse coordinate specified in an option. Can be in the form
 *   - chromCol - chrom in this column followed by start and end.
 *   - chromCol,startCol,endCol - chrom, start, and end in specified
 *    columns.
 *   - chromCol,startCol,endCol,strandCol - chrom, start,, end, and
 *     strand in specified columns.
 */

struct coordColVals coordColParseRow(struct coordCols* cols,
                                     struct rowReader *rr);
/* parse coords from a row */

#endif
