/* libScan - Scan libraries to help find g' capped ones. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "libScan - Scan libraries to help find g' capped ones\n"
  "usage:\n"
  "   libScan XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void libScan(char *XXX)
/* libScan - Scan libraries to help find g' capped ones. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
libScan(argv[1]);
return 0;
}
