/* splitFile - Split a file into pieces. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitFile - Split a file into pieces\n"
  "usage:\n"
  "   splitFile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void splitFile(char *XXX)
/* splitFile - Split a file into pieces. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
splitFile(argv[1]);
return 0;
}
