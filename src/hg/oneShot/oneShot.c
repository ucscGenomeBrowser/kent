/* oneShot - Select random ESTs from database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "oneShot - Select random ESTs from database\n"
  "usage:\n"
  "   oneShot XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void oneShot(char *XXX)
/* oneShot - Select random ESTs from database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
oneShot(argv[1]);
return 0;
}
