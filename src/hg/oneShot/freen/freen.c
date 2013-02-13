/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "rainbow.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen val desiredVal\n");
}

void freen(char *input, char *desiredOutput)
/* Test some hair-brained thing. */
{
double a = atof(input);
double desired = atof(desiredOutput);
double exponent = log(desired)/log(a);
printf("a = %g, desired = %g, a^%g = %g\n", a, desired, exponent, pow(a, exponent));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}
