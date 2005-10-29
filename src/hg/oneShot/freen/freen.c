/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "obscure.h"
#include "dnautil.h"
#include "jksql.h"
#include "visiGene.h"

static char const rcsid[] = "$Id: freen.c,v 1.58 2005/10/29 17:30:15 kent Exp $";

void usage()
{
errAbort("freen - test some hair brained thing.\n"
         "usage:  freen now\n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct dyString *dy = dyStringNew(0);
printf("%.s\n", fileName);
dyStringPrintf(dy, "%0.4s", fileName);
puts(dy->string);
dyStringFree(&dy);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
