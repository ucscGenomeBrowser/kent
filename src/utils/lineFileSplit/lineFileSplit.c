/* lineFileSplit - Split up a line oriented file into parts. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"

static char const rcsid[] = "$Id: lineFileSplit.c,v 1.1 2005/11/11 01:32:34 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lineFileSplit - Split up a line oriented file into parts\n"
  "usage:\n"
  "   lineFileSplit inFile how count outRoot\n"
  "where how is either 'files' or 'lines'\n"
  "In the case of files, count files of approximately equal lengths will\n"
  "be generated.  In the case of lines as many files as it takes each of \n"
  "count lines will be generated.\n"
  "   The output will be written to files starting with the name\n"
  "outRoot plus a numerical suffix.  The outRoot can include a directory\n"
  "and the directory will be created if it doesn't already exist.\n"
  "   As an example, to split the file 'big' into 100 roughly equal parts\n"
  "which are simply numbered files in the directory 'parts' do:\n"
  "   lineFileSplit big files 100 parts/\n"
  );
}

static struct optionSpec options[] = {
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

if (!sameWord(how, "files") && !sameString(how, "lines"))
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
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
lineFileSplit(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
