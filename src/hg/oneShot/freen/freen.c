/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"
#include "chain.h"

static char const rcsid[] = "$Id: freen.c,v 1.65 2006/05/05 18:45:30 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct chain *chainList = NULL, *chain;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while ((chain = chainRead(lf)) != NULL)
    slAddHead(&chainList, chain);
printf("Got %d chains\n", slCount(chainList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
