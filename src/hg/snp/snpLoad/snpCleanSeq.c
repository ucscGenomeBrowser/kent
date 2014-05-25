/* snpCleanSeq - clean fasta header lines to be compatible with hgLoadSeq. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"
#include "linefile.h"


static struct hash *snpHash = NULL;

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
struct hashEl *hel = NULL;
boolean skipping = FALSE;

outputFileHandle = mustOpen(outputFileName, "w");
lf = lineFileOpen(inputFileName, TRUE);

while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	skipping = FALSE;
        chopString(line, "|", row, ArraySize(row));
        chopString(row[2], " ", rsId, ArraySize(rsId));
        hel = hashLookup(snpHash, rsId[0]);
	if (hel)
	    skipping = TRUE;
	else
	    {
	    hashAdd(snpHash, cloneString(rsId[0]), NULL);
            fprintf(outputFileHandle, ">%s\n", rsId[0]);
	    }
	}
    else if (!skipping)
        fprintf(outputFileHandle, "%s\n", line);
    }
carefulClose(&outputFileHandle);
lineFileClose(&lf);
}



int main(int argc, char *argv[])
{
if (argc != 3)
    usage();

snpHash = newHash(16);
doCleanSeq(argv[1], argv[2]);

return 0;
}
