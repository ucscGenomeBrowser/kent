/* hgPhMouse - Load phMouse track. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPhMouse - Load phMouse track\n"
  "usage:\n"
  "   hgPhMouse XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgPhMouse(char *XXX)
/* hgPhMouse - Load phMouse track. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgPhMouse(argv[1]);
return 0;
}
