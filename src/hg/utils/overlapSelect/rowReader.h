/* rowReader - read rows from tab files or databases without length restrictions */
#ifndef ROWREADER_H
#define ROWREADER_H

struct rowReader
/* object to read rows */
{
    int numCols;          /* number of columns in current row */
    char **row;           /* parsed row */
    int colSpace;         /* number of columns allocated in row */
    struct lineFile *lf;  /* if reading from a file */
};

struct rowReader *rowReaderOpen(char *fileName, boolean isPslFile);
/* create a row reader for a file */

void rowReaderFree(struct rowReader **rrPtr);
/* free a row reader */

boolean rowReaderNext(struct rowReader *rr);
/* read the next row into the rowReader, return FALSE on EOF. */

void rowReaderExpectAtLeast(struct rowReader *rr, int minCols);
/* generate an error if there are not enough columns in a row */

char **rowReaderCloneColumns(struct rowReader *rr);
/* Make dynamic copy of columns currently in reader.  Vector is NULL terminated
 * and should be released with a single freeMem call. Must be called before
 * autoSql parser is called on a row, as it will modify array rows. */
#endif

