/* tableSum - Summarize a table somehow. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "options.h"


char *colDiv;
char *rowDiv;
double scale = 1.0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tableSum - Summarize a table somehow\n"
  "usage:\n"
  "   tableSum tableFile\n"
  "options:\n"
  "   -row         Sum all rows\n"
  "   -col         Sum all columns\n"
  "   -colEven=N         Output columns that are sum of N columns of input\n"
  "   -rowEven=N         Output rows that are sum of N rows of input\n"
  "   -colDiv=30,60,10  Produce table that sums columns first\n"
  "                     30 columns in input to first column in output,\n"
  "                     next 60 columns to second column in output,\n"
  "                     and next 10 columns to third column in output.\n"
  "   -rowDiv=30,60,10  Similar to colDiv, but for rows,  may be combined\n"
  "   -scale=X          Multiply everything by X\n"
  "   -average          Compute average instead of sum\n"
  );
}

int countWordsInFirstRealRow(char *fileName)
/* Count the number of rows in first line of file
 * that is nonblank and not-starting with # */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
int count = 0;
boolean gotLine = FALSE;

/* Get first non-blank line. */
while (lineFileNext(lf, &line, NULL))
    {
    line = skipLeadingSpaces(line);
    if (line != NULL && line[0] != 0 && line[0] != '#')
	{
	gotLine = TRUE;
        break;
	}
    }
if (!gotLine)
    errAbort("%s has no non-commented non-blank lines", fileName);
while ((word = nextWord(&line)) != NULL)
    {
    ++count;
    }
lineFileClose(&lf);
return count;
}

void tableFileDimensions(char *fileName, int *retX, int *retY)
/* Return dimensions of table. */
{
int x = countWordsInFirstRealRow(fileName);
int y = 0;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char **row;

AllocArray(row, x);
while (lineFileNextRow(lf, row, x))
    ++y;
lineFileClose(&lf);
*retX = x;
*retY = y;
}

void parseCuts(int inCount, char *spec, int **retCuts, int *retOutCount)
/* Parse comma delimited string of bin widths into array of
 * numerical bin boundaries. */
{
char **words;
int num, sum = 0, cut, *cuts;
int wordCount, commaCount, i;

commaCount = countChars(spec, ',') + 1;
AllocArray(words, commaCount);
wordCount = chopByChar(spec, ',', words, commaCount);
if (wordCount < 1)
    errAbort("Need at least one number in cut spec");
AllocArray(cuts, wordCount);
for (i=0; i<wordCount; ++i)
    {
    char *s = words[i];
    if (s[0] != '-' && !isdigit(s[0]))
        errAbort("Expecting number got %s", s);
    num = atoi(s);
    sum += num;
    cut = sum;
    if (cut > inCount)
        cut = inCount;
    cuts[i] = cut;
    }
freeMem(words);
*retCuts = cuts;
*retOutCount = wordCount;
}

char *evenDivisions(int inSize, int divSize)
/* Make a string divSize,divSize,divSize */
{
struct dyString *dy = dyStringNew(512);
char *s;
int cut;
for (cut=0; cut<inSize; cut += divSize)
    {
    if (cut != 0)
        dyStringPrintf(dy, ",");
    dyStringPrintf(dy, "%d", divSize);
    }
s = cloneString(dy->string);
dyStringFree(&dy);
return s;
}

void tableSum(char *table)
/* tableSum - Summarize a table somehow. */
{
struct lineFile *lf = lineFileOpen(table, TRUE);
int xInDim, yInDim, x = 0, y = 0;
char **row;
char buf[64];
int *xCuts, xOutDim, *yCuts, yOutDim, xCutIx, yCutIx;
double *rowSum;
boolean doAverage = optionExists("ave") || optionExists("average");

tableFileDimensions(table, &xInDim, &yInDim);

/* Set up default output,  which user may override
 * from command line. */
sprintf(buf, "%d", xInDim);
colDiv = cloneString(buf);
colDiv = optionVal("colDiv", colDiv);
sprintf(buf, "%d", yInDim);
rowDiv = cloneString(buf);
rowDiv = optionVal("rowDiv", rowDiv);
if (optionExists("colEven"))
    colDiv = evenDivisions(xInDim, optionInt("colEven", 0));
if (optionExists("rowEven"))
    rowDiv = evenDivisions(yInDim, optionInt("rowEven", 0));
if (optionExists("col") || optionExists("column"))
    colDiv = evenDivisions(xInDim, 1);
if (optionExists("row"))
    rowDiv = evenDivisions(yInDim, 1);

/* Parse ascii representation of output specification into cut list. */
parseCuts(xInDim, colDiv, &xCuts, &xOutDim);
parseCuts(yInDim, rowDiv, &yCuts, &yOutDim);

AllocArray(row, xInDim);
AllocArray(rowSum, xInDim);

for (yCutIx = 0; yCutIx < yOutDim; ++yCutIx)
    {
    int yEnd = yCuts[yCutIx];
    int yStart = y;
    for (x=0; x<xInDim; ++x)
        rowSum[x] = 0;
        
    for (; y<yEnd; ++y)
        {
	if (!lineFileNextRow(lf, row, xInDim))
	    errAbort("Unexpected end of file in %s\n", lf->fileName);
	for (x=0; x<xInDim; ++x)
	    rowSum[x] += atof(row[x]);
	}
    x = 0;
    for (xCutIx=0; xCutIx < xOutDim; ++xCutIx)
        {
	double sum = 0.0;
	int xEnd = xCuts[xCutIx];
	int xStart = x;
	for (; x<xEnd; ++x)
	    sum += rowSum[x];
	sum *= scale;
	if (doAverage)
	    {
	    int dx = yEnd - yStart;
	    int dy = xEnd - xStart;
	    int binCount = (dx) * (dy);
	    if (binCount > 0)
		sum /= binCount;
	    }
	printf("%f ", sum);
	}
    printf("\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
scale = optionFloat("scale", scale);
if (argc != 2)
    usage();
tableSum(argv[1]);
return 0;
}
