/* hgExtFile - Add external file to database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExtFile - Add external file to database\n"
  "usage:\n"
  "   hgExtFile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgExtFile(char *XXX)
/* hgExtFile - Add external file to database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
hgExtFile(argv[1]);
return 0;
}
