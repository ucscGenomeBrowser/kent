/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "dnaseq.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: freen.c,v 1.62 2005/12/07 17:11:58 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen now\n");
}


void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct twoBitFile *tbf = twoBitOpen(fileName);
long long totalSize = twoBitTotalSize(tbf);
printf("%s contains %lld bases\n", fileName, totalSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
