/* addFinFinf - Add list of finished clones to end of finf file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "addFinFinf - Add list of finished clones to end of finf file.\n"
  "usage:\n"
  "   addFinFinf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void addFinFinf(char *XXX)
/* addFinFinf - Add list of finished clones to end of finf file.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
addFinFinf(argv[1]);
return 0;
}
