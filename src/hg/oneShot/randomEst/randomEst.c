/* randomEst - Select random ESTs from database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "randomEst - Select random ESTs from database\n"
  "usage:\n"
  "   randomEst XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void randomEst(char *XXX)
/* randomEst - Select random ESTs from database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
randomEst(argv[1]);
return 0;
}
