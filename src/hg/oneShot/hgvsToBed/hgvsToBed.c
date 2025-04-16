/* hgvsToBed - Translate an HGVS term into BED coordinates if possible. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hgHgvs.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgvsToBed - Translate an HGVS term into BED coordinates if possible\n"
  "usage:\n"
  "   hgvsToBed <db> <HGVS>\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void hgvsToBed(char *db, char *hgvsTerm)
/* hgvsToBed - Translate an HGVS term into BED coordinates if possible. */
{
struct hgvsVariant *hgvs = hgvsParseTerm(hgvsTerm);
if (hgvsValidate(db, hgvs, NULL, NULL, NULL)) //char **retFoundAcc, int *retFoundVersion,
//                     char **retDiffRefAllele);
    {
    struct bed *output = hgvsMapToGenome(db, hgvs, NULL); //char **retPslTable);
    bedTabOut(output, stdout);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgvsToBed(argv[1], argv[2]);
return 0;
}
