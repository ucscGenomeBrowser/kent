/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "txCommon.h"


static char const rcsid[] = "$Id: freen.c,v 1.83 2008/04/28 13:08:46 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen id \n");
}

void freen(char *id)
/* Test some hair-brained thing. */
{
int x = atoi(id);
char buf[16];
txGeneAccFromId(x, buf);
printf("%s\n", buf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
