/* freen - My Pet Freen. */
#include "common.h"
#include "options.h"
#include "zlibFace.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}



void freen(char *a)
/* Test some hair-brained thing. */
{
uglyf("%s\n", simplifyPathToDir(a));
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
