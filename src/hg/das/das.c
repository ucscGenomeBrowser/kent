/* das - Das Server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "das - Das Server\n"
  "usage:\n"
  "   das XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void das(char *XXX)
/* das - Das Server. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
das(argv[1]);
return 0;
}
