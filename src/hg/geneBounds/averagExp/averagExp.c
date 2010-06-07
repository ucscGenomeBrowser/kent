/* averagExp - Average expression data within a cluster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: averagExp.c,v 1.2 2003/05/06 07:22:17 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "averagExp - Average expression data within a cluster\n"
  "usage:\n"
  "   averagExp XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void averagExp(char *XXX)
/* averagExp - Average expression data within a cluster. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
averagExp(argv[1]);
return 0;
}
