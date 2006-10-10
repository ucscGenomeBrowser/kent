/* snpReadSeq2 - Read dbSNP fasta files; find offset for each rsId. */

#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: snpReadSeq2.c,v 1.1 2006/10/10 20:58:14 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpReadSeq2 - Read dbSNP fasta file and log rsId offset.\n"
  "usage:\n"
  "  snpReadSeq2 inputfile \n");
}


void getOffset(char *inputFileName)
{
FILE *outputFileHandle = mustOpen("snpReadSeq2.out", "w");
struct lineFile *lf = lineFileOpen(inputFileName, TRUE);
char *line;
int lineSize;
off_t offset;
char *row[9], *rsId[2];

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	chopString(line, "|", row, ArraySize(row));
        chopString(row[2], " ", rsId, ArraySize(rsId));
	offset = lineFileTell(lf);
	fprintf(outputFileHandle, "%s\t%d\n", rsId[0], (int)offset);
	}
    }

carefulClose(&outputFileHandle);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
{
char *inputFileName = NULL;

if (argc != 2)
    usage();

inputFileName = argv[1];
getOffset(inputFileName);

return 0;
}
