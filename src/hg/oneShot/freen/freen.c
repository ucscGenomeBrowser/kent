/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "chain.h"

static char const rcsid[] = "$Id: freen.c,v 1.53 2004/11/23 00:23:12 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *a)
/* Test some hair-brained thing. */
{
struct chain *chainList = NULL, *chain;
struct lineFile *lf = lineFileOpen(a, TRUE);

pushCarefulMemHandler(2000000000);
while ((chain = chainRead(lf)) != NULL)
    {
    slAddHead(&chainList, chain);
    }
printf("%ld allocated in %d chains\n", carefulTotalAllocated(), slCount(chainList));
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
