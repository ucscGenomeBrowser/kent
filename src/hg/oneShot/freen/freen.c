/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "binRange.h"
#include "chromBins.h"


static char const rcsid[] = "$Id: freen.c,v 1.84 2008/08/27 22:34:06 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen id \n");
}

void freen(char *id)
/* Test some hair-brained thing. */
{
struct chromBins *chromBins = chromBinsNew(NULL);
struct binKeeper *bk = chromBinsGet(chromBins, "chr1", TRUE);
printf("%d bins\n", bk->binCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
