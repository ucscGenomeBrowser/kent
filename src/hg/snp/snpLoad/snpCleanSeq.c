/* snpCleanSeq - clean fasta header lines to be compatible with hgLoadSeq. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"
#include "linefile.h"

static char const rcsid[] = "$Id: snpCleanSeq.c,v 1.1 2006/11/14 20:25:07 heather Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "snpCleanSeq - clean fasta header lines to be compatible with hgLoadSeq.\n"
  "usage:\n"
  "  snpCleanSeq inputFilename outputFilename\n");
}


void doCleanSeq(char *inputFileName, char *outputFileName)
{
FILE *outputFileHandle = NULL;
struct lineFile *lf;
char *line;
char *row[9], *rsId[2];

outputFileHandle = mustOpen(outputFileName, "w");
lf = lineFileOpen(inputFileName, TRUE);

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
        chopString(line, "|", row, ArraySize(row));
        chopString(row[2], " ", rsId, ArraySize(rsId));
        fprintf(outputFileHandle, ">%s\n", rsId[0]);
	}
    else
        fprintf(outputFileHandle, "%s\n", line);
    }
carefulClose(&outputFileHandle);
lineFileClose(&lf);
}


int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

doCleanSeq(argv[1], argv[2]);

return 0;
}
