/* padFile - Add a bunch of zeroes to end of file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "padFile - Add a bunch of zeroes to end of file\n"
  "usage:\n"
  "   padFile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void padFile(char *XXX)
/* padFile - Add a bunch of zeroes to end of file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
padFile(argv[1]);
return 0;
}
