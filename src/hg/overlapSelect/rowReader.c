/* rowReader - read rows from tab files or databases without length restrictions */
#include "common.h"
#include "rowReader.h"
#include "linefile.h"
#include "psl.h"

struct rowReader *rowReaderOpen(char *fileName, boolean isPslFile)
/* create a row reader for a file */
{
struct rowReader *rr;
AllocVar(rr);
if (isPslFile)
    rr->lf = pslFileOpen(fileName); // handles psl headers
else
    rr->lf = lineFileOpen(fileName, TRUE);
rr->colSpace = 32;
AllocArray(rr->row, rr->colSpace);
return rr;
}

void rowReaderFree(struct rowReader **rrPtr)
/* free a row reader */
{
struct rowReader *rr = *rrPtr;
if (rr != NULL)
    {
    lineFileClose(&rr->lf);
    freeMem(rr->row);
    freeMem(rr);
    *rrPtr = NULL;
    }
}

static void growRow(struct rowReader *rr, int needCols)
/* grow buffer so it can hold the need number of columns */
{
int newSpace = rr->colSpace;
while (newSpace < needCols)
    newSpace *= 2;
ExpandArray(rr->row, rr->colSpace, newSpace);
rr->colSpace = newSpace;
}

static boolean rowReaderNextFile(struct rowReader *rr)
/* read the next row into the rowReader from a file. */
{
char *line;
if (!lineFileNextReal(rr->lf, &line))
    return FALSE;
rr->numCols = chopString(line, "\t", NULL, 0);
if (rr->numCols < rr->colSpace)
    growRow(rr, rr->numCols);
chopString(line, "\t", rr->row, rr->numCols);
return TRUE;
}

boolean rowReaderNext(struct rowReader *rr)
/* read the next row into the rowReader, return FALSE on EOF. */
{
return rowReaderNextFile(rr);
}

void rowReaderExpectAtLeast(struct rowReader *rr, int minCols)
/* generate an error if there are not enough columns in a row */
{
if (rr->numCols < minCols)
    errAbort("%s:%d: expected at least %d columns, got %d", 
             rr->lf->fileName, rr->lf->lineIx, minCols, rr->numCols);
}

