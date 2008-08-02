/* hgChroms - print chromosomes for a genome. */
#include "common.h"
#include "hdb.h"
#include "options.h"

static char const rcsid[] = "$Id: hgChroms.c,v 1.3.114.1 2008/08/02 04:06:20 markd Exp $";

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
    "   -noRandom - omit random and Un chromsomes\n"
    "   -noHap - omit _hap0 chromsomes\n"
    "   -noPrefix - omit \"chr\" prefix\n"
  );
}


/* command line */
static struct optionSpec optionSpec[] = {
    {"noRandom", OPTION_BOOLEAN},
    {"noHap", OPTION_BOOLEAN},
    {"noPrefix", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean noRandom;
boolean noHap;
boolean noPrefix;

bool inclChrom(struct slName *chrom)
/* check if a chromosome should be included */
{
return  !((noRandom && (endsWith(chrom->name, "_random")
                        || startsWith("chrUn", chrom->name)))
          || (noHap && stringIn( "_hap", chrom->name)));
}

void hgChroms(char *db)
/* hgChroms - print chromosomes for a genome. */
{
struct slName *chrom, *chroms = hAllChromNames(db);
for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    {
    if (inclChrom(chrom))
        {
        if (noPrefix && startsWith("chr", chrom->name))
            {
            printf("%s\n", chrom->name + strlen("chr"));
            }
        else
            {
            printf("%s\n", chrom->name);
            }
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 2)
    usage();
noRandom = optionExists("noRandom");
noHap = optionExists("noHap");
noPrefix = optionExists("noPrefix");
hgChroms(argv[1]);
return 0;
}
