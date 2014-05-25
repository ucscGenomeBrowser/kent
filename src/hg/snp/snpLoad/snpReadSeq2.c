/* snpReadSeq2 - Read dbSNP fasta files; find offset for each rsId. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpReadSeq2 - Read dbSNP fasta file and log rsId offset.\n"
  "usage:\n"
  "  snpReadSeq2 directory chrom outputfile\n");
}


void getOffset(char *directoryName, char *chromName, char *outputFileName)
{
FILE *outputFileHandle = mustOpen(outputFileName, "w");
struct lineFile *lf = NULL;
char *line;
off_t offset;
char *row[9], *rsId[2];
char inputFileName[64];

safef(inputFileName, sizeof(inputFileName), "%s/%s.fa", directoryName, chromName);
lf = lineFileOpen(inputFileName, TRUE);
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	chopString(line, "|", row, ArraySize(row));
        chopString(row[2], " ", rsId, ArraySize(rsId));
	offset = lineFileTell(lf);
	fprintf(outputFileHandle, "%s\t%s\t%ld\n", rsId[0], chromName, offset);
	}
    }

carefulClose(&outputFileHandle);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *directoryName = NULL;
char *chromName = NULL;
char *outputFileName = NULL;

if (argc != 4)
    usage();

directoryName = argv[1];
chromName = argv[2];
outputFileName = argv[3];
getOffset(directoryName, chromName, outputFileName);

return 0;
}
