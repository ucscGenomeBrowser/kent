/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "twoBit.h"
#include "portable.h"

static char const rcsid[] = "$Id: freen.c,v 1.51 2004/10/23 15:08:00 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *a)
/* Test some hair-brained thing. */
{
long start = clock1000();
struct twoBitFile *tbf = twoBitOpen(a);
long openTime = clock1000();
struct dnaSeq *seq = twoBitReadSeqFrag(tbf, "scaffold_1000", 0, 0);
long seqTime = clock1000();
twoBitClose(&tbf);
long end = clock1000();
printf("Took %ld millis to twoBitOpen %s\n", openTime - start, a);
printf("Took %ld millis to load %s (%d bases)\n", seqTime - openTime, seq->name, seq->size);
printf("Took %ld millis to twoBitClose %s\n", end - seqTime, a);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
