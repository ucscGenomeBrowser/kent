/* spacedColumn - stuff to handle parsing text files where fields are
 * fixed width rather than tab delimited. */

#include "common.h"
#include "linefile.h"
#include "spacedColumn.h"
#include "obscure.h"
#include "sqlNum.h"


struct spacedColumn *spacedColumnFromSample(char *sample)
/* Return spaced column list from a sampleline , which is assumed to
 * have no spaces except between columns */
{
struct spacedColumn *col, *colList = NULL;
char *dupe = cloneString(sample);
char *word, *line = dupe;
while ((word = nextWord(&line)) != NULL)
    {
    AllocVar(col);
    col->start = word - dupe;
    col->size = strlen(word);
    slAddHead(&colList, col);
    }
freeMem(dupe);
slReverse(&colList);
return colList;
}

struct spacedColumn *spacedColumnFromLineFile(struct lineFile *lf)
/* Scan through lineFile and figure out column spacing. Assumes
 * file contains nothing but columns. */
{
int maxLine = 64*1024;
int lineSize, widestLine = 0;
char *projection = needMem(maxLine+1);
char *line;
struct spacedColumn *colList;
int i;

/* Create projection of all lines. */
for (i=0; i<maxLine; ++i)
    projection[i] = ' ';
while (lineFileNext(lf, &line, &lineSize))
    {
    if (lineSize > widestLine)
         widestLine = lineSize;
    for (i=0; i<lineSize; ++i)
        {
	char c = line[i];
	if (c != 0 && c != ' ')
	    projection[i] = line[i];
	}
    }
projection[widestLine] = 0;
colList = spacedColumnFromSample(projection);
freeMem(projection);
return colList;
}

struct spacedColumn *spacedColumnFromFile(char *fileName)
/* Read file and figure out where columns are. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct spacedColumn *colList = spacedColumnFromLineFile(lf);
lineFileClose(&lf);
return colList;
}

int spacedColumnBiggestSize(struct spacedColumn *colList)
/* Return size of biggest column. */
{
int maxSize = 0;
struct spacedColumn *col;
for (col = colList; col != NULL; col = col->next)
    if (maxSize < col->size)
        maxSize = col->size;
return maxSize;
}

boolean spacedColumnParseLine(struct spacedColumn *colList, 
	char *line, char *row[])
/* Parse line into row according to colList.  This will
 * trim leading and trailing spaces. It will write 0's
 * into line.  Returns FALSE if there's a problem (like
 * line too short.) */
{
struct spacedColumn *col;
int i, len = strlen(line);
for (i=0, col = colList; col != NULL; col = col->next, ++i)
    {
    if (col->start > len)
	return FALSE;
    int end = col->start + col->size;
    if (end > len) end = len;
    line[end] = 0;
    row[i] = trimSpaces(line + col->start);
    }
return TRUE;
}

struct spacedColumn *spacedColumnFromWidthArray(int array[], int size)
/* Return a list of spaced columns corresponding to widths in array.
 * The final char in each column should be whitespace. */
{
struct spacedColumn *col, *colList = NULL;
int i;
int start = 0;
for (i=0; i<size; ++i)
    {
    int width = array[i];
    AllocVar(col);
    col->start = start;
    col->size = width-1;
    slAddHead(&colList, col);
    start += width;
    }
slReverse(&colList);
return colList;
}

struct spacedColumn *spacedColumnFromSizeCommaList(char *commaList)
/* Given an comma-separated list of widths in ascii, return
 * a list of spacedColumns. */
{
struct slName *ascii, *asciiList = commaSepToSlNames(commaList);
int colCount = slCount(asciiList);
int widths[colCount], i;
for (ascii = asciiList, i=0; ascii != NULL; ascii = ascii->next, ++i)
    widths[i] = sqlUnsigned(ascii->name);
slFreeList(&asciiList);
return spacedColumnFromWidthArray(widths, colCount);
}

