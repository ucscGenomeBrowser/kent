/* dsn - DSN Server for DAS. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dsn - DSN Server for DAS\n"
  "usage:\n"
  "   dsn XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dsn(char *XXX)
/* dsn - DSN Server for DAS. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
dsn(argv[1]);
return 0;
}
