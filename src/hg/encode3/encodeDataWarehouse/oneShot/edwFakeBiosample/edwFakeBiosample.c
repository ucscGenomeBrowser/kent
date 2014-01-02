/* edwFakeBiosample - Fake up biosample table from meld of encode3 and encode2 sources.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFakeBiosample - Fake up biosample table from meld of encode3 and encode2 sources.\n"
  "usage:\n"
  "   edwFakeBiosample cellTypeSlim.tab edwBiosample.tab\n"
  "Where cellTypeSlim.tab is the result of running the command on hgwdev\n"
  "    hgsql -e 'select term,sex,organism from cellType' -N encode2Meta\n"
  "and also lives in the source tree.  The output is intended for the edwBiosample table.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFakeBiosample(char *input, char *output)
/* edwFakeBiosample - Fake up biosample table from meld of encode3 and encode2 sources.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[3];
while (lineFileRow(lf, row))
    {
    char *term = row[0];
    char *sex = row[1];
    char *organism = row[2];
    unsigned taxon = 0;
    if (sameWord(organism, "human")) taxon = 9606;
    else if (sameWord(organism, "mouse")) taxon = 10090;
    else errAbort("Unrecognized organism %s", organism);
    fprintf(f, "0\t%s\t%u\t%s\n",  term, taxon, sex);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFakeBiosample(argv[1], argv[2]);
return 0;
}
