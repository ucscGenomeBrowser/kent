/* knownVsBlat - Categorize BLAT mouse hits to known genes. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "knownVsBlat - Categorize BLAT mouse hits to known genes\n"
  "usage:\n"
  "   knownVsBlat XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void knownVsBlat(char *XXX)
/* knownVsBlat - Categorize BLAT mouse hits to known genes. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
knownVsBlat(argv[1]);
return 0;
}
