/* splitFile - Split up a file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splitFile - Split up a file\n"
  "usage:\n"
  "   splitFile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void splitFile(char *XXX)
/* splitFile - Split up a file. */
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
