#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "sqlNum.h"
#include "matrixMarket.h"

void matrixMarketJustNumberCoordinates(struct matrixMarket *mm, char *fileName)
/* Make sure that the matrix is of the type we can handle */
{
if (differentWord(mm->format, "coordinate"))
    errAbort("%s is not a MatrixMarket matrix in coordinate format", fileName);
if (differentWord(mm->type, "integer") && differentWord(mm->type, "real"))
    errAbort("%s has numbers in %s format, can only handle real and integer",
	fileName, mm->type);
if (differentWord(mm->symmetries, "general"))
    errAbort("%s has %s symmetries, can only handle general symmetries", 
        fileName, mm->symmetries);
}

struct matrixMarket *matrixMarketOpen(char *fileName)
/* Open a matrixMarket file and read header, return a handle for use in more routines. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[6];	// Big enough for header
int rowSize;

/* Get header line, check for errors */
rowSize = lineFileChopNext(lf, row, ArraySize(row));
lineFileExpectWords(lf, 5, rowSize);
if (differentString(row[0], "%%MatrixMarket"))
   errAbort("%s is not a MatrixMarket file", fileName);
if (differentWord(row[1], "matrix"))
    errAbort("%s is not a MatrixMarket matrix", fileName);

/* Skip over other lines that start with % */
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] != '%')
        {
	lineFileReuse(lf);
	break;
	}
    }

/* Allocate our return object and fill in what we can from the header line */
struct matrixMarket *mm;
AllocVar(mm);
mm->lf = lf;
mm->format = cloneString(row[2]);
mm->type = cloneString(row[3]);
mm->symmetries = cloneString(row[4]);

/* Precalculate a shortcut or two */
mm->isInt = sameWord(mm->type, "integer");

/* Get the dimensions line and save it in our data structure */
rowSize = lineFileChopNext(lf, row, ArraySize(row));
lineFileExpectWords(lf, 3, rowSize);
mm->colCount = lineFileNeedNum(lf, row, 0);
mm->rowCount = lineFileNeedNum(lf, row, 1);
mm->valCount = lineFileNeedNum(lf, row, 2);
if (mm->colCount <= 0 || mm->rowCount <= 0)
    errAbort("%d x %d matrix in %s, dimensions must be positive)", mm->colCount, mm->rowCount, fileName);

return mm;
}

void matrixMarketClose(struct matrixMarket **pMm)
/* Close down matrix market and free up associated resources */
{
struct matrixMarket *mm = *pMm;
if (mm != NULL)
    {
    lineFileClose(&mm->lf);
    freeMem(mm->format);
    freeMem(mm->type);
    freeMem(mm->symmetries);
    freeMem(mm->row);
    freez(pMm);
    }
}


boolean matrixMarketNext(struct matrixMarket *mm)
/* Fetch next value from matrix market, returning FALSE if at end of file */
{
char *row[4];
struct lineFile *lf = mm->lf;
int rowSize = lineFileChopNext(lf, row, ArraySize(row));
if (rowSize <= 0)
    return FALSE;
lineFileExpectWords(lf, 3, rowSize);
mm->x = sqlUnsigned(row[0]) - 1;
if (mm->x >= mm->colCount)
    errAbort("X coordinate %d too big, %d max, line %d of %s", mm->x+1, mm->colCount, 
	lf->lineIx, lf->fileName);
mm->y = sqlUnsigned(row[1]) - 1;
if (mm->y >= mm->rowCount)
    errAbort("Y coordinate %d too big, %d max, line %d of %s", mm->y+1, mm->rowCount, 
	lf->lineIx, lf->fileName);
char *val = row[2];
mm->val = (mm->isInt ? sqlSigned(val) : sqlDouble(val));
return TRUE;
}

boolean matrixMarketNextRow(struct matrixMarket *mm)
/* Fetch next row into mm->row, return FALSE at eof.  Beware this may skip rows
 * so look at mm->rowIx as well */
{
if (mm->lastRow)
    return FALSE;
if (mm->row == NULL)
    {
    if (!matrixMarketNext(mm))   /* We always start one into the row - this catches empty matrix*/
        return FALSE;
    mm->rowIx = mm->y;
    AllocArray(mm->row, mm->colCount);
    }
zeroBytes(mm->row, mm->colCount * sizeof(mm->row[0]));
mm->rowIx = mm->y;
mm->row[mm->x] = mm->val;
for (;;)
    {
    if (!matrixMarketNext(mm))
        {
	mm->lastRow = TRUE;
	break;
	}
    if (mm->y != mm->rowIx)
        {
	if (mm->y < mm->rowIx)
	    errAbort("%s is not a sorted-by-y matrix-market on line %d", mm->lf->fileName, 
		mm->lf->lineIx);
	break;
	}
    mm->row[mm->x] = mm->val;
    }
return TRUE;
}

