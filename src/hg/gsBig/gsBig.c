/* gsBig - Run Genscan on big input and produce GTF files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsBig - Run Genscan on big input and produce GTF files\n"
  "usage:\n"
  "   gsBig XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void gsBig(char *XXX)
/* gsBig - Run Genscan on big input and produce GTF files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
gsBig(argv[1]);
return 0;
}
