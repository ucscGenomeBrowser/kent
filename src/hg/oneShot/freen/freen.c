/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"

static char const rcsid[] = "$Id: freen.c,v 1.50 2004/10/12 14:19:52 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void freen(char *a)
/* Test some hair-brained thing. */
{
struct sqlConnection *conn = sqlConnect("hg17");
int count = sqlNeedQuickNum(conn, "select max(size) from chromInfo where chrom like 'chr%_random%'");
printf("Count = %d\n", count);;
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
