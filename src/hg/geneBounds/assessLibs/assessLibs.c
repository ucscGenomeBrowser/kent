/* assessLibs - Make table that assesses the percentage of library that covers 5' and 3' ends. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "assessLibs - Make table that assesses the percentage of library that covers 5' and 3' ends\n"
  "usage:\n"
  "   assessLibs XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void assessLibs(char *XXX)
/* assessLibs - Make table that assesses the percentage of library that covers 5' and 3' ends. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
assessLibs(argv[1]);
return 0;
}
