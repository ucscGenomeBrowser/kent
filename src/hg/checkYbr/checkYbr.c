/* checkYbr - Check NCBI assembly (aka Yellow Brick Road). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkYbr - Check NCBI assembly (aka Yellow Brick Road)\n"
  "usage:\n"
  "   checkYbr XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void checkYbr(char *XXX)
/* checkYbr - Check NCBI assembly (aka Yellow Brick Road). */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
checkYbr(argv[1]);
return 0;
}
