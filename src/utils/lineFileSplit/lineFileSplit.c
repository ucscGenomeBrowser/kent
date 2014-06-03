/* lineFileSplit - Split up a line oriented file into parts. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"


int sizeColumn = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lineFileSplit - Split up a line oriented file into parts\n"
  "usage:\n"
  "   lineFileSplit inFile how count outRoot\n"
  "where how is either 'files' or 'lines' or 'column'\n"
  "In the case of files, count files of approximately equal lengths will\n"
  "be generated.  In the case of lines as many files as it takes each of \n"
  "count lines will be generated. In the case of column asecond column\n"
  "of the file is assumed to be a number, and it will split up the file\n"
  "so as to equalize the sum of this number in the output to make approximately\n"
  "count files\n"
  "   The output will be written to files starting with the name\n"
  "outRoot plus a numerical suffix.  The outRoot can include a directory\n"
  "and the directory will be created if it doesn't already exist.\n"
  "   As an example, to split the file 'big' into 100 roughly equal parts\n"
  "which are simply numbered files in the directory 'parts' do:\n"
  "   lineFileSplit big files 100 parts/\n"
  "options:\n"
  "   column=n - Used when how is column.  Specifies which column has the\n"
  "              number in it. By default this will be column 1.\n"
  );
}

static struct optionSpec options[] = {
   {"column", OPTION_INT},
   {NULL, 0},
};

void lineFileSplit(char *inFile, char *how, char *countString, char *outRoot)
/* lineFileSplit - Split up a line oriented file into parts. */
{
struct slName *lineList, *line;
int lineCount;
int count = atoi(countString);
char outFile[PATH_LEN];
char dir[256], file[128], suffix[64];
int fileIx = 0;

if (!sameWord(how, "files") && !sameString(how, "lines")
	&& !sameWord(how, "column"))
    usage();
if (count <= 0)
    usage();
lineList = readAllLines(inFile);
line = lineList;
lineCount = slCount(lineList);
splitPath(outRoot, dir, file, suffix);
if (dir[0] != 0)
    makeDir(dir);

if (sameWord(how, "files"))
    {
    int startLine = 0, endLine, thisFileLineCount;
    int digitsNeeded = digitsBaseTen(count);
    for (fileIx=1; fileIx <= count; fileIx += 1)
        {
	int i;
	FILE *f;
	endLine = fileIx * lineCount / count;
	thisFileLineCount = endLine - startLine;
	startLine = endLine;
	safef(outFile, sizeof(outFile),
		"%s/%s%0*d%s", dir, file, digitsNeeded, fileIx, suffix);
	f = mustOpen(outFile, "w");
	for (i = 0; i<thisFileLineCount; ++i)
	    {
	    fprintf(f, "%s\n", line->name);
	    line = line->next;
	    }
	carefulClose(&f);
	}
    }
else if (sameWord(how, "lines"))
    {
    int outFileCount = (lineCount + count - 1)/count;
    int digitsNeeded = digitsBaseTen(outFileCount);
    for (fileIx=1; fileIx<= outFileCount; ++fileIx)
        {
	int i;
	FILE *f;
	safef(outFile, sizeof(outFile),
		"%s/%s%0*d%s", dir, file, digitsNeeded, fileIx, suffix);
	f = mustOpen(outFile, "w");
	for (i = 0; i<count && line != NULL; ++i)
	    {
	    fprintf(f, "%s\n", line->name);
	    line = line->next;
	    }
	carefulClose(&f);
	}

    }
else if (sameWord(how, "column"))
    {
    char **row, *numString;
    int wordsInLine, columnIx = sizeColumn-1;
    struct dyString *dy = dyStringNew(0);
    AllocArray(row, sizeColumn);
    int lineIx = 1;
    long curSize = 0, totalSize = 0;
    int digitsNeeded = digitsBaseTen(count);

    /* Figure out total size. */
    for (line = lineList; line != NULL; line = line->next, ++lineIx)
        {
	dyStringClear(dy);
	dyStringAppend(dy, line->name);
	wordsInLine = chopByWhite(dy->string, row, sizeColumn);
	if (wordsInLine < sizeColumn)
	    errAbort("Not enough columns line %d of %s", lineIx, inFile);
	numString = row[columnIx];
	if (!isdigit(numString[0]))
	    errAbort("Expecting number got %s in column %d, line %d of %s", 
	         sizeColumn, lineIx, inFile);
	totalSize += atoi(numString);
	}
    verbose(1, "Total of column %d is %ld\n", sizeColumn, totalSize);

    line = lineList;
    for (fileIx=1; fileIx <= count && line != NULL; ++fileIx)
        {
	FILE *f;

	/* Figure out how much we want to have written into this file. */
	double curRatio = (double)fileIx / count;
	long targetSize = curRatio * totalSize;
	if (fileIx == count)
	    targetSize = totalSize;  /* Avoid problems from rounding error. */

	/* Open file and write. */
	safef(outFile, sizeof(outFile),
		"%s/%s%0*d%s", dir, file, digitsNeeded, fileIx, suffix);
	f = mustOpen(outFile, "w");
	while (line != NULL)
	    {
	    dyStringClear(dy);
	    dyStringAppend(dy, line->name);
	    wordsInLine = chopByWhite(dy->string, row, sizeColumn);
	    fprintf(f, "%s\n", line->name);
	    line = line->next;
	    curSize += atoi(row[columnIx]);
	    if (curSize >= targetSize)
	        break;
	    }
	carefulClose(&f);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
sizeColumn = optionInt("column", sizeColumn);
lineFileSplit(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
