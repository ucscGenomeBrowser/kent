/* whyConserved - Try and analyse why a particular thing is conserved. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "whyConserved - Try and analyse why a particular thing is conserved\n"
  "usage:\n"
  "   whyConserved XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void whyConserved(char *XXX)
/* whyConserved - Try and analyse why a particular thing is conserved. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
whyConserved(argv[1]);
return 0;
}
