/* stringify - Convert file to C strings. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stringify - Convert file to C strings\n"
  "usage:\n"
  "   stringify XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void stringify(char *XXX)
/* stringify - Convert file to C strings. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
stringify(argv[1]);
return 0;
}
