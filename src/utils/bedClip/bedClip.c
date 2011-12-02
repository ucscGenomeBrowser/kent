/* bedClip - Remove lines from bed file that refer to off-chromosome places.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bbiFile.h"
#include "sqlNum.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedClip - Remove lines from bed file that refer to off-chromosome places.\n"
  "usage:\n"
  "   bedClip input.bed chrom.sizes output.bed\n"
  "options:\n"
  "   -verbose=2 - set to get list of lines clipped and why\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void bedClip(char *inFile, char *chromSizes, char *outFile)
/* bedClip - Remove lines from bed file that refer to off-chromosome places.. */
{
struct hash *chromSizesHash = bbiChromSizesFromFile(chromSizes);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *line;
while (lineFileNextReal(lf, &line))
    {
    char *chrom = nextWord(&line);
    char *startString = nextWord(&line);
    char *endString = nextWord(&line);
    if (endString == NULL)
        errAbort("Need at least three fields line %d of %s", lf->lineIx, lf->fileName);
    if (startString[0] == '-')
	{
	verbose(2, "Clipping negative line %d of %s\n", lf->lineIx, lf->fileName);
        continue;		// Clip off negatives
	}
    if (!isdigit(startString[0]))
        errAbort("Expecting number got %s line %d of %s", startString, lf->lineIx, lf->fileName);
    if (!isdigit(endString[0]))
        errAbort("Expecting number got %s line %d of %s", endString, lf->lineIx, lf->fileName);
    int start = sqlUnsigned(startString);
    int end = sqlUnsigned(endString);
    if (start >= end)
	{
	verbose(2, "Clipping end <= start line %d of %s\n", lf->lineIx, lf->fileName);
	continue;
	}
    struct hashEl *hel = hashLookup(chromSizesHash, chrom);
    if (hel == NULL)
        errAbort("Chromosome %s isn't in %s line %d of %s\n", chrom, chromSizes, lf->lineIx, lf->fileName);
    int chromSize = ptToInt(hel->val);
    if (end > chromSize)
	{
	verbose(2, "Clipping end > chromSize line %d of %s\n", lf->lineIx, lf->fileName);
	continue;
	}
    fprintf(f, "%s\t%s\t%s", chrom, startString, endString);
    line = skipLeadingSpaces(line);
    if (line == NULL || line[0] == 0)
        fputc('\n', f);
    else
        fprintf(f, "\t%s\n", line);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedClip(argv[1], argv[2], argv[3]);
return 0;
}
