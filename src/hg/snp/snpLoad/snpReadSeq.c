/* snpReadSeq - Read dbSNP fasta files. */
/* Split into individual files with a limit of files per directory. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
// needed for makeDir
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpReadSeq - Read dbSNP fasta files and split into directories.\n"
  "usage:\n"
  "  snpReadSeq inputfile logfile outputdir filesPerDirectory \n");
}


void splitSeqFile(char *inputFileName, char *logFileName, char *outputDirBasename, int filesPerDir)
{
FILE *outputFileHandle = NULL;
FILE *logFileHandle = mustOpen(logFileName, "w");
char outputFileName[64];
char dirName[64];
int fileCount = 0;
int dirCount = 0;
struct lineFile *lf = lineFileOpen(inputFileName, TRUE);
char *line;
int lineSize;
boolean firstLine = TRUE;
char *row[9], *rsId[2];

safef(dirName, sizeof(dirName), "%s-%d", outputDirBasename, dirCount);
makeDir(dirName);

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	if (!firstLine)
	    carefulClose(&outputFileHandle);
	else
	    firstLine = FALSE;
        fileCount++;
	if (fileCount == filesPerDir)
	    {
	    fileCount = 0;
	    dirCount++;
            safef(dirName, sizeof(dirName), "%s-%d", outputDirBasename, dirCount);
            makeDir(dirName);
	}
	/* use rsId for filename */
	chopString(line, "|", row, ArraySize(row));
	chopString(row[2], " ", rsId, ArraySize(rsId));
        safef(outputFileName, sizeof(outputFileName), "%s/%s", dirName, rsId[0]);
        outputFileHandle = mustOpen(outputFileName, "w");
        fprintf(logFileHandle, "%s\t%s/%s\n", rsId[0], dirName, rsId[0]);
	}
    else
       fprintf(outputFileHandle, "%s\n", line);
    }

carefulClose(&outputFileHandle);
carefulClose(&logFileHandle);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *inputFileName = NULL;
char *logFileName = NULL;
char *outputDirBasename = NULL;
int filesPerDir = 0;

if (argc != 5)
    usage();

inputFileName = argv[1];
logFileName = argv[2];
outputDirBasename = argv[3];
filesPerDir = atoi(argv[4]);

splitSeqFile(inputFileName, logFileName, outputDirBasename, filesPerDir);

return 0;
}
