/* spacedToTab - Convert fixed width space separated fields to tab separated. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: spacedToTab.c,v 1.2 2007/03/23 06:02:42 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spacedToTab - Convert fixed width space separated fields to tab separated\n"
  "Note this requires two passes, so it can't be done on a pipe\n"
  "usage:\n"
  "   spacedToTab in.txt out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct spacedColumn
/* Specs on a column. */
    {
    struct spacedColumn *next;
    int start;	/* Starting index. */
    int size;	/* Size of column. */
    };

struct spacedColumn *spacedColumnListFromSample(char *sample)
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

struct spacedColumn *spacedColumnListFromLineFile(struct lineFile *lf)
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
colList = spacedColumnListFromSample(projection);
freeMem(projection);
return colList;
}

struct spacedColumn *spacedColumnListFromFile(char *fileName)
/* Read file and figure out where columns are. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct spacedColumn *colList = spacedColumnListFromLineFile(lf);
lineFileClose(&lf);
return colList;
}

int spacedColumnListBiggestSize(struct spacedColumn *colList)
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

void spacedToTab(char *inFile, char *outFile)
/* spacedToTab - Convert fixed width space separated fields to tab separated. */
{
struct spacedColumn *colList = spacedColumnListFromFile(inFile);
int colCount = slCount(colList);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *line;
int lineSize;
FILE *f = mustOpen(outFile, "w");
char *row[colCount];

while (lineFileNext(lf, &line, &lineSize))
    {
    if (!spacedColumnParseLine(colList, line, row))
         errAbort("Short line %d of %s\n", lf->lineIx, lf->fileName);
    int i, lastCol = colCount-1;
    for (i = 0; i<=lastCol; ++i)
        {
	fputs(row[i], f);
	if (i == lastCol)
	    fputc('\n', f);
	else
	    fputc('\t', f);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
spacedToTab(argv[1],argv[2]);
return 0;
}
