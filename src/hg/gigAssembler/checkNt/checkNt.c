/* checkNt - Check that ctg_nt.agp, ctg_coords, and ctg.fa are consistent. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkNt - Check that ctg_nt.agp, ctg_coords, and ctg.fa are consistent\n"
  "usage:\n"
  "   checkNt XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void checkNt(char *XXX)
/* checkNt - Check that ctg_nt.agp, ctg_coords, and ctg.fa are consistent. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
checkNt(argv[1]);
return 0;
}
