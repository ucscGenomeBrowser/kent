/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "autoXml - Generate structures code and parser for XML file from DTD-like spec\n"
  "usage:\n"
  "   autoXml XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void autoXml(char *XXX)
/* autoXml - Generate structures code and parser for XML file from DTD-like spec. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
autoXml(argv[1]);
return 0;
}
