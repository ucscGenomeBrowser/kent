/* hgsql - Execute some sql code using passwords in .hg.conf. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsql - Execute some sql code using passwords in .hg.conf\n"
  "usage:\n"
  "   hgsql XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgsql(char *XXX)
/* hgsql - Execute some sql code using passwords in .hg.conf. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
hgsql(argv[1]);
return 0;
}
