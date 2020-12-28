/* csvToTsv - Convert a comma separated value file to tab separate value file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "csv.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "csvToTsv - Convert a comma separated value file to tab separate value file\n"
  "usage:\n"
  "   csvToTsv in.csv out.tsv\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void csvToTsv(char *inFile, char *outFile)
/* csvToTsv - Convert a comma separated value file to tab separate value file. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dyString *scratch = dyStringNew(0);
int maxFieldSize = 1000000;
char *escapeBuf = needLargeMem(maxFieldSize * 2);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *pos = line;
    char *val;
    boolean firstInLine = TRUE;
    while ((val = csvParseNext(&pos, scratch)) != NULL)
        {
	if (firstInLine)
	    firstInLine = FALSE;
	else
	    fputc('\t', f);
	int valLen = strlen(val);
	if (valLen > maxFieldSize)
	    {
	    errAbort("csvToTsv can't handle field of %d bytes line %d of %s", valLen, lf->lineIx, lf->fileName);
	    }
        escCopy(val, escapeBuf, '\t', '\\');
	fprintf(f, "%s", escapeBuf);
	}
    fputc('\n', f);
    int err = ferror(f);
    if (err != 0)
        {
	errAbort("Error writing line %d of %s: %s\n", lf->lineIx, outFile, strerror(err));
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
csvToTsv(argv[1], argv[2]);
return 0;
}
