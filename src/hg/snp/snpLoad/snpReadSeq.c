/* snpReadSeq - Read dbSNP fasta files. */
/* Split into individual files with a limit of files per directory. */

#include "common.h"
#include "dystring.h"
#include "linefile.h"
// needed for makeDir
#include "portable.h"

static char const rcsid[] = "$Id: snpReadSeq.c,v 1.1 2006/09/23 11:07:52 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpReadSeq - Read dbSNP fasta files and split into directories.\n"
  "usage:\n"
  "  snpReadSeq inputfile outputdir filesPerDirectory \n");
}


void splitSeqFile(char *inputFileName, char *outputDirBasename, int filesPerDir)
{
FILE *inputFileHandle = mustOpen(inputFileName, "r");
FILE *outputFileHandle = NULL;
char outputFileName[64];
char dirName[64];
int fileCount = 0;
int dirCount = 0;
struct lineFile *lf = lineFileOpen(inputFileName, TRUE);
char *line;
int lineSize;
boolean firstLine = TRUE;
char *row[9], *rsId[2];

safef(dirName, sizeof(dirName), "%s%d", outputDirBasename, dirCount);
makeDir(dirName);
safef(outputFileName, sizeof(outputFileName), "%s/split%d", dirName, fileCount);
outputFileHandle = mustOpen(outputFileName, "w");

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>' && !firstLine)
        {
	carefulClose(&outputFileHandle);
        fileCount++;
	if (fileCount == filesPerDir)
	    {
	    fileCount = 0;
	    dirCount++;
            safef(dirName, sizeof(dirName), "%s%d", outputDirBasename, dirCount);
            makeDir(dirName);
	}
        safef(outputFileName, sizeof(outputFileName), "%s/split%d", dirName, fileCount);
        outputFileHandle = mustOpen(outputFileName, "w");
	}
    if (firstLine)
        firstLine = FALSE;
    fprintf(outputFileHandle, "%s\n", line);
    }

// does order matter?
carefulClose(&inputFileHandle);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *inputFileName = NULL;
char *outputDirBasename = NULL;
int filesPerDir = 0;

if (argc != 4)
    usage();

inputFileName = argv[1];
outputDirBasename = argv[2];
filesPerDir = atoi(argv[3]);

splitSeqFile(inputFileName, outputDirBasename, filesPerDir);

return 0;
}
