/* checkGoldDupes - Check gold files in assembly for duplicates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkGoldDupes - Check gold files in assembly for duplicates\n"
  "usage:\n"
  "   checkGoldDupes XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void checkGoldDupes(char *XXX)
/* checkGoldDupes - Check gold files in assembly for duplicates. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
checkGoldDupes(argv[1]);
return 0;
}
