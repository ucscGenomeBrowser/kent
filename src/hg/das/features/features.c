/* features - DAS Annotation Feature Server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "features - DAS Annotation Feature Server\n"
  "usage:\n"
  "   features XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void features(char *XXX)
/* features - DAS Annotation Feature Server. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
features(argv[1]);
return 0;
}
