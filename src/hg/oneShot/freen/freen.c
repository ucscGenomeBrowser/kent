/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "correlate.h"

static char const rcsid[] = "$Id: freen.c,v 1.68 2006/07/01 07:09:14 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *fileName)
/* Test some hair-brained thing. */
{
static double x[] = {1, 2, 1, 2, 1, 2, 1};
static double y[] = {2, 4, 2, 4, 2, 4, 2};
static double z[] = {-1,-2,-1,-2,-1,-2,-1};
static double r[] = {1, 2, 3, 4, 5, 6, 7};
printf("r(xy) = %f\n", correlateArrays(x, y, 7));
printf("r(xz) = %f\n", correlateArrays(x, z, 7));
printf("r(xr) = %f\n", correlateArrays(x, r, 7));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
