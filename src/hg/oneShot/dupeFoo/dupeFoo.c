/* dupeFoo - Do some duplication analysis. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dupeFoo - Do some duplication analysis\n"
  "usage:\n"
  "   dupeFoo XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dupeFoo(char *XXX)
/* dupeFoo - Do some duplication analysis. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
dupeFoo(argv[1]);
return 0;
}
