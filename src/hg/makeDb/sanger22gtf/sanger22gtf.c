/* sanger22gtf - Convert Sanger chromosome 22 annotations to gtf. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sanger22gtf - Convert Sanger chromosome 22 annotations to gtf\n"
  "usage:\n"
  "   sanger22gtf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void sanger22gtf(char *XXX)
/* sanger22gtf - Convert Sanger chromosome 22 annotations to gtf. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
sanger22gtf(argv[1]);
return 0;
}
