/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "correlate.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *track)
/* Test some hair-brained thing. */
{
struct correlate *c = correlateNew();
int i;
for (i=0; i<6; ++i)
    correlateNext(c, i&1, 1-(i&1));
correlateNext(c, 4, 4);
printf("1 in 7 correlation is %g\n", correlateResult(c));
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
