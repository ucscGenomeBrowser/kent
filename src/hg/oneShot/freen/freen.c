/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"
#include "liftOver.h"

static char const rcsid[] = "$Id: freen.c,v 1.61 2005/11/26 17:25:21 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen now\n");
}


void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct liftOverChain *lift = liftOverChainForDb("hg17");
uglyf("Got %d lifts\n", slCount(lift));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
