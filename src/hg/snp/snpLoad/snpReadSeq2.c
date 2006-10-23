/* snpReadSeq2 - Read dbSNP fasta files; find offset for each rsId. */

#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: snpReadSeq2.c,v 1.3 2006/10/23 22:04:18 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpReadSeq2 - Read dbSNP fasta file and log rsId offset.\n"
  "usage:\n"
  "  snpReadSeq2 inputfile outputfile\n");
}


void getOffset(char *inputFileName, char *outputFileName)
{
FILE *outputFileHandle = mustOpen(outputFileName, "w");
struct lineFile *lf = lineFileOpen(inputFileName, TRUE);
char *line;
int lineSize;
off_t offset;
char *row[9], *rsId[2];

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	chopString(line, "|", row, ArraySize(row));
        chopString(row[2], " ", rsId, ArraySize(rsId));
	offset = lineFileTell(lf);
	fprintf(outputFileHandle, "%s\t%ld\n", rsId[0], offset);
	}
    }

carefulClose(&outputFileHandle);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *inputFileName = NULL;
char *outputFileName = NULL;

if (argc != 3)
    usage();

inputFileName = argv[1];
outputFileName = argv[2];
getOffset(inputFileName, outputFileName);

return 0;
}
