/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "botDelay.h"

static char const rcsid[] = "$Id: freen.c,v 1.40 2004/02/05 18:39:52 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *in)
/* Test some hair-brained thing. */
{
botDelayMessage("localhost", 123);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
