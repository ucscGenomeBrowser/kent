/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "jksql.h"
#include "mysqlTableStatus.h"

static char const rcsid[] = "$Id: freen.c,v 1.44 2004/06/03 22:56:12 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *in)
/* Test some hair-brained thing. */
{
char *pt = needMem(1025);
char *buf = NULL;
free(pt);
free(pt);
free(buf + 1024);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
