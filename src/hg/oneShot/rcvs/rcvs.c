/* rcvs - Compare riken noncoding vs. nonspliced. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rcvs - Compare riken noncoding vs. nonspliced\n"
  "usage:\n"
  "   rcvs XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void rcvs(char *XXX)
/* rcvs - Compare riken noncoding vs. nonspliced. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
rcvs(argv[1]);
return 0;
}
