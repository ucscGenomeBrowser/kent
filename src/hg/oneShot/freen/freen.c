/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "dnautil.h"

static char const rcsid[] = "$Id: freen.c,v 1.56 2005/01/10 00:41:12 kent Exp $";

void usage()
{
errAbort("freen - test some hair brained thing.\n"
         "usage:  freen now\n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
printf("log 2 = %f\n", log(2));
printf("1/log 2 = %f\n", 1.0/log(2));
printf("33%%100 = %d\n", 33%100);
printf("133%%100 = %d\n", 133%100);
printf("-33%%100 = %d\n", -33%100);
printf("-133%%100 = %d\n", -33%100);
printf("33u%%100u = %d\n", 33u%100u);
printf("133u%%100u = %d\n", 133u%100u);
printf("-33u%%100u = %d\n", (unsigned)-33%100u);
printf("-133u%%100u = %d\n", (unsigned)-33%100u);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
