/* venn - Do venn diagram calculations. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "venn - Do venn diagram calculations\n"
  "usage:\n"
  "   venn XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void venn(char *XXX)
/* venn - Do venn diagram calculations. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
venn(argv[1]);
return 0;
}
