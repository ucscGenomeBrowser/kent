/* hgChroms - print chromosomes for a genome. */
#include "common.h"
#include "hdb.h"
#include "options.h"

static char const rcsid[] = "$Id: hgChroms.c,v 1.1 2004/01/21 06:04:08 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
    "hgChroms - print chromosomes for a genome.\n"
    "\n"
    "usage:\n"
    "   hgChroms [options] db\n"
    "\n"
    "Options:\n"
    "   -noRandom - omit random chromsomes\n"
  );
}


/* command line */
static struct optionSpec optionSpec[] = {
    {"noRandom", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean noRandom;

void hgChroms(char *db)
/* hgChroms - print chromosomes for a genome. */
{
struct slName *chrom, *chroms = hAllChromNamesDb(db);
for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    {
    if ((!noRandom) || (strstr(chrom->name, "_random") == NULL))
        printf("%s\n", chrom->name);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 2)
    usage();
noRandom = optionExists("noRandom");
hgChroms(argv[1]);
return 0;
}
