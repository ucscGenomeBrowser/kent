/* pepPredToFa - Convert a pepPred table to fasta format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pepPredToFa - Convert a pepPred table to fasta format\n"
  "usage:\n"
  "   pepPredToFa XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pepPredToFa(char *XXX)
/* pepPredToFa - Convert a pepPred table to fasta format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
pepPredToFa(argv[1]);
return 0;
}
