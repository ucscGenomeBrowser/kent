/* bedIntersect - Intersect two bed files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedIntersect - Intersect two bed files\n"
  "usage:\n"
  "   bedIntersect XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void bedIntersect(char *XXX)
/* bedIntersect - Intersect two bed files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
bedIntersect(argv[1]);
return 0;
}
