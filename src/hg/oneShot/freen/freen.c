/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "hdb.h"


static char const rcsid[] = "$Id: freen.c,v 1.80 2007/12/21 18:56:07 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
printf("sizeof double = %d\n", (int)sizeof(double));
printf("sizeof float = %d\n", (int)sizeof(float));
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
