/* homoPhile - A program that looks for boundaries between genes by looking at homology, especially EST, evidence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "homoPhile - A program that looks for boundaries between genes by looking at homology, especially EST, evidence\n"
  "usage:\n"
  "   homoPhile XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void homoPhile(char *XXX)
/* homoPhile - A program that looks for boundaries between genes by looking at homology, especially EST, evidence. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
homoPhile(argv[1]);
return 0;
}
