/* pepPredToFa - Convert a pepPred table to fasta format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: pepPredToFa.c,v 1.2 2003/06/10 16:10:03 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pepPredToFa - Convert a pepPred table to fasta format\n"
  "usage:\n"
  "   pepPredToFa database table\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void pepPredToFa(char *database, char *table)
/* pepPredToFa - Convert a pepPred table to fasta format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pepPredToFa(argv[1], argv[2]);
return 0;
}
