/* liftPromoHits - Lift motif hits from promoter to chromosome coordinates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftPromoHits - Lift motif hits from promoter to chromosome coordinates\n"
  "usage:\n"
  "   liftPromoHits XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void liftPromoHits(char *XXX)
/* liftPromoHits - Lift motif hits from promoter to chromosome coordinates. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
liftPromoHits(argv[1]);
return 0;
}
