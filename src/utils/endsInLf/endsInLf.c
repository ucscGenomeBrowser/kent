/* endsInLf - Check that last letter in files is end of line. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "endsInLf - Check that last letter in files is end of line\n"
  "usage:\n"
  "   endsInLf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void endsInLf(char *XXX)
/* endsInLf - Check that last letter in files is end of line. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
endsInLf(argv[1]);
return 0;
}
