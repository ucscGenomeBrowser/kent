/* hgNear - Functional neighborhood gene browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: hgNear.c,v 1.1 2003/06/15 18:22:54 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNear - Functional neighborhood gene browser\n"
  "usage:\n"
  "   hgNear XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgNear(char *XXX)
/* hgNear - Functional neighborhood gene browser. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
hgNear(argv[1]);
return 0;
}
