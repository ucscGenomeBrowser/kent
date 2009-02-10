/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "binRange.h"
#include "hdb.h"


static char const rcsid[] = "$Id: freen.c,v 1.85 2009/02/10 22:22:08 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file\n");
}

void freen(char *asciiCount)
/* Test some hair-brained thing. */
{
int count = atoi(asciiCount);
int i;
for (i=0; i<count; ++i)
    {
    printf("%d\t0.1\n", rand()%100);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
