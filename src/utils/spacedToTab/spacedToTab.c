/* spacedToTab - Convert fixed width space separated fields to tab separated. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: spacedToTab.c,v 1.1 2003/09/24 11:24:53 kent Exp $";

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

struct column
/* Specs on a column. */
    {
    struct column *next;
    int start;	/* Starting index. */
    int size;	/* Size of column. */
    };

struct column *figureColumns(char *fileName)
/* Read file and figure out where columns are. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int maxLine = 64*1024;
int lineSize, widestLine = 0;
char *projection = needMem(maxLine+1);
char *line, *word;
struct column *colList = NULL, *col;
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
lineFileClose(&lf);
projection[widestLine] = 0;

/* Chop up projection by white space. */
line = projection;
while ((word = nextWord(&line)) != NULL)
    {
    AllocVar(col);
    col->start = word - projection;
    col->size = strlen(word);
    slAddHead(&colList, col);
    }
slReverse(&colList);
return colList;
}

int biggestColSize(struct column *colList)
/* Return size of biggest column. */
{
int maxSize = 0;
struct column *col;
for (col = colList; col != NULL; col = col->next)
    if (maxSize < col->size)
        maxSize = col->size;
return maxSize;
}

void spacedToTab(char *inFile, char *outFile)
/* spacedToTab - Convert fixed width space separated fields to tab separated. */
{
struct column *col, *colList = figureColumns(inFile);
int maxSize = biggestColSize(colList);
char *buf = needMem(maxSize+1);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *line, *word;
int lineSize;
FILE *f = mustOpen(outFile, "w");

while (lineFileNext(lf, &line, &lineSize))
    {
    for (col = colList; col != NULL; col = col->next)
        {
	if (col->start < lineSize)
	    {
	    memcpy(buf, line + col->start, col->size);
	    buf[col->size] = 0;
	    word = trimSpaces(buf);
	    fputs(word, f);
	    }
	if (col->next == NULL)
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
