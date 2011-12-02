/* tabRow - a row from a database or a tab-separated file held in
 * memory.   Just a light wrapper around an array of strings. 
 * Also some routines to convert slLines to tabRows. */

#include "common.h"
#include "tabRow.h"


struct tabRow *tabRowNew(int colCount)
/* Return new row. */
{
struct tabRow *row = needMem(sizeof(*row) + colCount*sizeof(char*));
row->colCount = colCount;
return row;
}

int tabRowMaxColCount(struct tabRow *rowList)
/* Return largest column count */
{
int maxCount = 0;
struct tabRow *row;
for (row = rowList; row != NULL; row = row->next)
    if (row->colCount > maxCount)
        maxCount = row->colCount;
return maxCount;
}

struct tabRow *tabRowByWhite(struct slName *lineList, char *fileName,
	boolean varCol)
/* Convert lines to rows based on spaces.  If varCol is TRUE then not
 * all rows need to have same number of columns. */
{
struct slName *line;
struct tabRow *rowList = NULL, *row;

if (varCol)
    {
    for (line = lineList; line != NULL; line = line->next)
        {
	char *s = line->name;
	int rowSize = chopByWhite(s, NULL, 0);
	row = tabRowNew(rowSize);
	chopByWhite(s, row->columns, rowSize);
	slAddHead(&rowList, row);
	}
    }
else
    {
    if (lineList)
        {
	int rowSize = chopByWhite(lineList->name, NULL, 0);
	int extraSize = rowSize+1;
	int ix = 1;
	for (line = lineList; line != NULL; line = line->next, ++ix)
	    {
	    int oneSize;
	    row = tabRowNew(rowSize);
	    oneSize = chopByWhite(line->name, row->columns, extraSize);
	    if (oneSize != rowSize)
	        {
		if (oneSize > rowSize)
		    errAbort("Got more than the expected %d columns line %d of %s",
			    rowSize, ix, fileName);
		else
		    errAbort("Expecting %d columns got %d, line %d of %s",
		    	rowSize, oneSize, ix, fileName);

		}
	    slAddHead(&rowList, row);
	    }
	}
    }
slReverse(&rowList);
return rowList;
}

struct tabRow *tabRowByChar(struct slName *lineList, char c, char *fileName,
	boolean varCol)
/* Convert lines to rows based on character separation.  If varCol is TRUE then not
 * all rows need to have same number of columns. */
{
struct slName *line;
struct tabRow *rowList = NULL, *row;

if (varCol)
    {
    for (line = lineList; line != NULL; line = line->next)
        {
	char *s = line->name;
	int rowSize = countChars(s, c) + 1;
	row = tabRowNew(rowSize);
	chopByChar(s, c, row->columns, rowSize);
	slAddHead(&rowList, row);
	}
    }
else
    {
    if (lineList)
        {
	int rowSize = countChars(lineList->name, c) + 1;
	int extraSize = rowSize+1;
	int ix = 1;
	for (line = lineList; line != NULL; line = line->next, ++ix)
	    {
	    int oneSize;
	    row = tabRowNew(rowSize);
	    oneSize = chopByChar(line->name, c, row->columns, extraSize);
	    if (oneSize != rowSize)
	        {
		if (oneSize > rowSize)
		    errAbort("Got more than the expected %d columns line %d of %s",
			    rowSize, ix, fileName);
		else
		    errAbort("Expecting %d columns got %d, line %d of %s",
		    	rowSize, oneSize, ix, fileName);

		}
	    slAddHead(&rowList, row);
	    }
	}
    }
return rowList;
}

struct slInt *tabRowGuessFixedOffsets(struct slName *lineList, char *fileName)
/* Return our best guess list of starting positions for space-padded fixed
 * width fields. */
{
struct slInt *offList = NULL, *off;

if (lineList)
    {
    char *spaceRec = cloneString(lineList->name), *s;
    int lineSize = strlen(spaceRec);
    struct slName *line;
    int lineIx=1;

    /* First 'or' together all lines into spaceRec, which will
     * have a space wherever all columns of all lines are space and
     * non-space elsewhere. */
    for (line = lineList->next; line != NULL; line = line->next, ++lineIx)
        {
	int i;
	s = line->name;
	if (strlen(s) != lineSize)
	   errAbort("Line %d of %s has %lu chars, but first line has just %d",
	       lineIx, fileName, (unsigned long)strlen(s), lineSize);
	for (i=0; i<lineSize; ++i)
	    {
	    if (s[i] != ' ')
	        spaceRec[i] = 'X';
	    }
	}

    /* Now make up slInt list that describes where words begin */
    s = spaceRec;
    for (;;)
        {
	s = skipLeadingSpaces(s);
	if (s == NULL || s[0] == 0)
	    break;
	AllocVar(off);
	off->val = s - spaceRec;
	slAddHead(&offList, off);
	s = skipToSpaces(s);
	}
    slReverse(&offList);
    }
return offList;
}

struct tabRow *tabRowByFixedOffsets(struct slName *lineList, struct slInt *offList,
	char *fileName)
/* Return rows parsed into fixed width fields whose starts are defined by
 * offList. */
{
struct slName *line;
struct slInt *off;
struct tabRow *rowList = NULL, *row;
int rowSize = slCount(offList);

if (lineList)
    {
    int lineSize = strlen(lineList->name);
    int lineIx = 1;
    for (off = offList; off != NULL; off = off->next)
        {
	if (off->val >= lineSize)
	    errAbort("Offset %d is bigger than lineSize of %d", off->val, lineSize);
	}
    for (line = lineList; line != NULL; line = line->next, ++lineIx)
	{
	char *linePt = line->name;
	int offIx = 0;
	if (strlen(linePt) != lineSize)
	   errAbort("Line %d of %s has %lu chars, but first line has just %d",
	       lineIx, fileName, (unsigned long)strlen(linePt), lineSize);
	row = tabRowNew(rowSize);
	for (off = offList; off != NULL; off = off->next, ++offIx)
	    {
	    int start = off->val, end;
	    if (off->next != NULL)
		{
	        end = off->next->val-1;
		if (linePt[end] != ' ')
		   errAbort("Line %d of %s expecting space column %d, got %c",
			   lineIx, fileName, end, linePt[end]);
		}
	    else
	        end = lineSize;
	    linePt[end] = 0;
	    row->columns[offIx] = trimSpaces(linePt + start);
	    }
	slAddHead(&rowList, row);
	}
    slReverse(&rowList);
    }
return rowList;
}

struct tabRow *tabRowByFixedGuess(struct slName *lineList, char *fileName)
/* Return rows parsed into fixed-width fields. */
{
struct slInt *offList = tabRowGuessFixedOffsets(lineList, fileName);
struct tabRow *rowList = tabRowByFixedOffsets(lineList, offList, fileName);
slFreeList(&offList);
return rowList;
}

