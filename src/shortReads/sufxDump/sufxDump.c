/* sufxDump - Dump out info on sufx array.  Useful for debugging. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
 
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "sufx.h"

static char const rcsid[] = "$Id: sufxDump.c,v 1.2 2008/10/28 23:34:51 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufxDump - Dump out info on sufx array.  Useful for debuggin.\n"
  "usage:\n"
  "   sufxDump input.sufx output.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void writeZeroSuppress(FILE *f, DNA *dna, int size)
/* Write out "dna" converting any non-dna chars to _ */
{
int i;
for (i=0; i<size; ++i)
    {
    int b = dna[i];
    if (b < 0 || ntVal[b] < 0)
        b = '_';
    fputc(b, f);
    }
}

void sufxDump(char *input, char *output)
/* sufxDump - Dump out info on sufx array.  Useful for debuggin.. */
{
struct sufx *sufx = sufxRead(input, FALSE);
FILE *f = mustOpen(output, "w");
int i;
for (i=0; i<sufx->header->arraySize; ++i)
    {
    fprintf(f, "%4d %4d ", i, sufx->traverse[i]);
    writeZeroSuppress(f, sufx->allDna+sufx->array[i], 40);
    fprintf(f, " %d\n", sufx->array[i]);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
sufxDump(argv[1], argv[2]);
return 0;
}
