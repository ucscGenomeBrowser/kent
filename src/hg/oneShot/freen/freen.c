/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "jksql.h"
#include "mysqlTableStatus.h"

static char const rcsid[] = "$Id: freen.c,v 1.46 2004/06/10 22:11:49 galt Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *in)
/* Test some hair-brained thing. */
{
char *pt = needMem(1025);
pt[1025] = 0;
int i;
for (i=0; i<100; ++i)
    pt = needMem(100000);
carefulCheckHeap();
pt[-1] = 0;
freeMem(pt);
}


int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
