/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"

static char const rcsid[] = "$Id: freen.c,v 1.76 2007/03/03 19:45:15 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}


void freen(char *aFile, char *bFile)
/* Test some hair-brained thing. */
{
struct bed *a = bedLoadAll(aFile);
struct bed *b = bedLoadAll(bFile);
int overlap = bedSameStrandOverlap(a,b);
printf("Overlap between %s and %s is %d\n", a->name, b->name, overlap);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
return 0;
}
